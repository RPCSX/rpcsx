#pragma once

#include "io-device.hpp"
#include "orbis-config.hpp"
#include "orbis/KernelAllocator.hpp"
#include "orbis/error.hpp"
#include "orbis/utils/IdMap.hpp"
#include "orbis/utils/Rc.hpp"
#include "orbis/utils/SharedAtomic.hpp"
#include "rx/refl.hpp"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <limits>
#include <mutex>
#include <print>
#include <stop_token>
#include <thread>
#include <utility>

namespace ajm {
enum ControlFlags {
  kControlInitialize = 0x4000,
  kControlReset = 0x2000,
};

enum RunFlags {
  kRunMultipleFrames = 0x1000,
  kRunGetCodecInfo = 0x800,
};

enum SidebandFlags {
  kSidebandStream = 0x800000000000,
  kSidebandFormat = 0x400000000000,
  kSidebandGaplessDecode = 0x200000000000,
};

enum {
  kResultInvalidData = 0x2,
  kResultInvalidParameter = 0x4,
  kResultPartialInput = 0x8,
  kResultNotEnoughRoom = 0x10,
  kResultStreamChange = 0x20,
  kResultTooManyChannels = 0x40,
  kResultUnsupportedFlag = 0x80,
  kResultSidebandTruncated = 0x100,
  kResultPriorityPassed = 0x200,
  kResultCodecError = 0x40000000,
  kResultFatal = 0x80000000,
};

enum class CodecId : std::uint32_t {
  MP3 = 0,
  At9 = 1,
  AAC = 2,
};

enum class ChannelCount : orbis::uint32_t {
  Default,
  _1,
  _2,
  _3,
  _4,
  _5,
  _6,
  _8,
};

enum class Format : orbis::uint32_t {
  S16 = 0, // default
  S32 = 1,
  Float = 2
};

enum class BatchId : std::uint32_t {};
enum class InstanceId : std::uint32_t {};

struct PackedInstanceId {
  std::uint32_t raw;

  static PackedInstanceId create(CodecId codecId, InstanceId instanceId) {
    return {.raw = static_cast<std::uint32_t>(codecId) << 15 |
                   static_cast<std::uint32_t>(instanceId)};
  }

  [[nodiscard]] InstanceId getInstanceId() const {
    return static_cast<InstanceId>(raw & ((1 << 15) - 1));
  }
  [[nodiscard]] CodecId getCodecId() const {
    return static_cast<CodecId>(raw >> 15);
  }

  auto operator<=>(const PackedInstanceId &) const = default;
};

struct SidebandGaplessDecode {
  orbis::uint32_t totalSamples;
  orbis::uint16_t skipSamples;
  orbis::uint16_t totalSkippedSamples;
};

struct SidebandResult {
  orbis::int32_t result;
  orbis::int32_t codecResult;
};

struct SidebandStream {
  orbis::int32_t inputSize;
  orbis::int32_t outputSize;
  orbis::uint64_t decodedSamples;
};

struct SidebandMultipleFrames {
  orbis::uint32_t framesProcessed;
  orbis::uint32_t unk0;
};

struct SidebandFormat {
  ChannelCount channels;
  orbis::uint32_t unk0; // maybe channel mask?
  orbis::uint32_t sampleRate;
  Format sampleFormat;
  uint32_t bitrate;
  uint32_t unk1;
};

struct AjmBuffer {
  orbis::ptr<std::byte> pData;
  orbis::size_t size;
};

struct Job {
  std::uint64_t flags;
  std::uint32_t sidebandSize;
  std::uint32_t totalOutputSize;
  std::vector<AjmBuffer> inputBuffers;
  std::vector<AjmBuffer> outputBuffers;
  orbis::ptr<std::byte> pSideband;
  orbis::ptr<SidebandResult> pSidebandResult;
  std::uint64_t controlFlags;
};

class InstanceBatch;

struct Batch : orbis::RcBase {
  enum Status {
    Queued,
    Cancelled,
    Complete,
  };

private:
  std::atomic<std::uint32_t> mCompleteJobs{0};
  orbis::shared_atomic32 mStatus{Status::Queued};
  std::atomic<std::uint64_t> mBatchError{0};
  std::vector<orbis::Ref<InstanceBatch>> mInstanceBatches;

public:
  [[nodiscard]] orbis::ErrorCode wait(std::uint32_t timeout,
                                      std::uint64_t *batchError) {
    std::chrono::microseconds usecTimeout;
    if (timeout == std::numeric_limits<std::uint32_t>::max()) {
      usecTimeout = std::chrono::microseconds::max();
    } else {
      usecTimeout = std::chrono::microseconds(timeout);
    }

    while (true) {
      auto status = mStatus.load(std::memory_order::relaxed);
      if (status != Status::Queued) {
        *batchError = mBatchError.load(std::memory_order::relaxed);
        break;
      }

      auto errc = mStatus.wait(status, usecTimeout);

      if (errc != std::errc{}) {
        return orbis::toErrorCode(errc);
      }
    }

    return {};
  }

  [[nodiscard]] Status getStatus() const {
    return Status(mStatus.load(std::memory_order::acquire));
  }

  void setStatus(Status status) {
    if (status == Status::Queued) {
      return;
    }

    std::uint32_t prevStatus = Status::Queued;
    if (mStatus.compare_exchange_strong(prevStatus, status,
                                        std::memory_order::relaxed,
                                        std::memory_order::release)) {
      mStatus.notify_all();
    }
  }

  void handleCompletion(std::uint64_t batchError) {
    if (mBatchError != 0) {
      std::uint64_t prevError = 0;
      mBatchError.compare_exchange_strong(prevError, batchError);
    }

    if (mCompleteJobs.fetch_add(1) == mInstanceBatches.size() - 1) {
      setStatus(Status::Complete);
    }
  }
};

class InstanceBatch : public orbis::RcBase {
  orbis::Ref<Batch> mBatch;
  std::vector<Job> mJobs;

public:
  InstanceBatch() = default;
  InstanceBatch(orbis::Ref<Batch> batch, std::vector<Job> jobs)
      : mBatch(std::move(batch)), mJobs(std::move(jobs)) {}

  std::span<const Job> getJobs() { return mJobs; }
  void complete(std::uint64_t batchError) {
    mBatch->handleCompletion(batchError);
  }
  [[nodiscard]] bool inProgress() const {
    return mBatch->getStatus() == Batch::Status::Queued;
  }
};

class CodecInstance : public orbis::RcBase {
  std::mutex mWorkerMutex;
  std::condition_variable mWorkerCv;
  std::deque<orbis::Ref<InstanceBatch>> mJobQueue;
  std::jthread mWorkerThread{
      [this](const std::stop_token &stopToken) { workerEntry(stopToken); }};

public:
  virtual ~CodecInstance() {
    std::lock_guard lock(mWorkerMutex);
    mWorkerThread.request_stop();
    mWorkerCv.notify_all();
  }

  virtual std::uint64_t runJob(const Job &job) = 0;
  virtual void reset() = 0;

  void runBatch(orbis::Ref<Batch> batch, std::vector<Job> jobs) {
    auto instanceBatch = orbis::Ref<InstanceBatch>(
        orbis::knew<InstanceBatch>(std::move(batch), std::move(jobs)));

    std::lock_guard lock(mWorkerMutex);
    mJobQueue.push_back(instanceBatch);
    mWorkerCv.notify_one();
  }

private:
  void workerEntry(const std::stop_token &stopToken) {
    while (!stopToken.stop_requested()) {
      orbis::Ref<InstanceBatch> batch;
      {
        std::unique_lock lock(mWorkerMutex);

        while (mJobQueue.empty()) {
          mWorkerCv.wait(lock);

          if (stopToken.stop_requested()) {
            return;
          }
        }
      }

      if (batch == nullptr) {
        continue;
      }

      if (batch->inProgress()) {
        std::uint64_t error = 0;
        for (auto &job : batch->getJobs()) {
          auto result = runJob(job);
          if (result != 0) {
            error = result;
          }
        }

        batch->complete(error);
      }
    }
  }
};

struct Codec : orbis::RcBase {
  virtual ~Codec() = default;
  [[nodiscard]] virtual orbis::ErrorCode
  createInstance(orbis::Ref<ajm::CodecInstance> *instance, std::uint32_t unk0,
                 std::uint64_t flags) = 0;
};

inline constexpr auto kCodecCount = rx::fieldCount<ajm::CodecId>;
} // namespace ajm

struct AjmDevice : IoDevice {
  orbis::shared_mutex mtx;

  orbis::Ref<ajm::Codec> codecs[ajm::kCodecCount];
  orbis::RcIdMap<ajm::CodecInstance, ajm::InstanceId>
      mCodecInstances[ajm::kCodecCount];

  orbis::RcIdMap<ajm::Batch, ajm::BatchId, 4096, 1> batchMap;

  orbis::ErrorCode open(orbis::Ref<orbis::File> *file, const char *path,
                        std::uint32_t flags, std::uint32_t mode,
                        orbis::Thread *thread) override;

  template <typename InstanceT, typename... ArgsT>
  void createCodec(ajm::CodecId id, ArgsT &&...args) {
    auto instance = orbis::knew<InstanceT>(std::forward<ArgsT>(args)...);
    codecs[std::to_underlying(id)] = instance;
  }

  [[nodiscard]] orbis::ErrorCode
  createInstance(orbis::Ref<ajm::CodecInstance> *instance, ajm::CodecId codecId,
                 std::uint32_t unk0, std::uint64_t flags) {
    auto rawCodecId = std::to_underlying(codecId);
    if (rawCodecId >= ajm::kCodecCount) {
      return orbis::ErrorCode::INVAL;
    }

    auto codec = codecs[rawCodecId];

    if (codec == nullptr) {
      return orbis::ErrorCode::SRCH;
    }

    return codec->createInstance(instance, unk0, flags);
  }

  [[nodiscard]] orbis::ErrorCode removeInstance(ajm::CodecId codecId,
                                                ajm::InstanceId instanceId) {
    auto rawCodecId = std::to_underlying(codecId);
    if (rawCodecId >= ajm::kCodecCount) {
      return orbis::ErrorCode::INVAL;
    }

    if (!mCodecInstances[rawCodecId].close(instanceId)) {
      return orbis::ErrorCode::BADF;
    }

    return {};
  }

  [[nodiscard]] orbis::Ref<ajm::CodecInstance>
  getInstance(ajm::CodecId codecId, ajm::InstanceId instanceId) {
    auto rawCodecId = std::to_underlying(codecId);
    if (rawCodecId >= ajm::kCodecCount) {
      return {};
    }

    return mCodecInstances[rawCodecId].get(instanceId);
  }

  [[nodiscard]] ajm::InstanceId
  addCodecInstance(ajm::CodecId codecId,
                   orbis::Ref<ajm::CodecInstance> instance) {
    auto &instances = mCodecInstances[std::to_underlying(codecId)];

    auto id = instances.insert(std::move(instance));

    if (id == std::remove_cvref_t<decltype(instances)>::npos) {
      std::println(stderr, "out of codec instances");
      std::abort();
    }

    return id;
  }

  [[nodiscard]] ajm::BatchId addBatch(ajm::Batch *batch) {
    auto id = batchMap.insert(batch);
    if (id == decltype(batchMap)::npos) {
      std::println(stderr, "out of batches");
      std::abort();
    }

    return id;
  }

  [[nodiscard]] orbis::Ref<ajm::Batch> getBatch(ajm::BatchId id) const {
    return batchMap.get(id);
  }

  [[nodiscard]] orbis::ErrorCode removeBatch(ajm::BatchId id) {
    if (batchMap.close(id)) {
      return orbis::ErrorCode::BADF;
    }

    return {};
  }
};
