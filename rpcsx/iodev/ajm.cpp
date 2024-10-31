#include "ajm.hpp"
#include "io-device.hpp"
#include "orbis-config.hpp"
#include "orbis/KernelAllocator.hpp"
#include "orbis/file.hpp"
#include "orbis/thread/Thread.hpp"
#include "orbis/utils/Logs.hpp"
#include <cstdint>
#include <cstring>
#include <print>
#include <rx/atScopeExit.hpp>
#include <rx/hexdump.hpp>

namespace ajm {
enum class Opcode : std::uint8_t {
  RunBufferRa = 1,
  ControlBufferRa = 2,
  Flags = 4,
  ReturnAddress = 6,
  JobBufferOutputRa = 17,
  JobBufferSidebandRa = 18,
};

struct InstructionHeader {
  orbis::uint32_t id;
  orbis::uint32_t len;
};

static_assert(sizeof(InstructionHeader) == 0x8);

struct OpcodeHeader {
  orbis::uint32_t opcode;

  [[nodiscard]] Opcode getOpcode() const {
    // ORBIS_LOG_ERROR(__FUNCTION__, opcode);
    if (auto loType = static_cast<Opcode>(opcode & 0xf);
        loType == Opcode::ReturnAddress || loType == Opcode::Flags) {
      return loType;
    }

    return static_cast<Opcode>(opcode & 0x1f);
  }
};

struct ReturnAddress {
  orbis::uint32_t opcode;
  orbis::uint32_t unk; // 0, padding?
  orbis::ptr<void> returnAddress;
};
static_assert(sizeof(ReturnAddress) == 0x10);

struct BatchJobControlBufferRa {
  orbis::uint32_t opcode;
  orbis::uint32_t sidebandInputSize;
  orbis::ptr<std::byte> pSidebandInput;
  orbis::uint32_t flagsHi;
  orbis::uint32_t flagsLo;
  orbis::uint32_t commandId;
  orbis::uint32_t sidebandOutputSize;
  orbis::ptr<std::byte> pSidebandOutput;

  std::uint64_t getFlags() { return ((uint64_t)flagsHi << 0x1a) | flagsLo; }
};
static_assert(sizeof(BatchJobControlBufferRa) == 0x28);

struct BatchJobInputBufferRa {
  orbis::uint32_t opcode;
  orbis::uint32_t szInputSize;
  orbis::ptr<std::byte> pInput;
};
static_assert(sizeof(BatchJobInputBufferRa) == 0x10);

struct BatchJobFlagsRa {
  orbis::uint32_t flagsHi;
  orbis::uint32_t flagsLo;
};

static_assert(sizeof(BatchJobFlagsRa) == 0x8);

struct BatchJobOutputBufferRa {
  orbis::uint32_t opcode;
  orbis::uint32_t outputSize;
  orbis::ptr<std::byte> pOutput;
};
static_assert(sizeof(BatchJobOutputBufferRa) == 0x10);

struct BatchJobSidebandBufferRa {
  orbis::uint32_t opcode;
  orbis::uint32_t sidebandSize;
  orbis::ptr<std::byte> pSideband;
};
static_assert(sizeof(BatchJobSidebandBufferRa) == 0x10);

static Job decodeJob(const std::byte *ptr, const std::byte *endPtr) {
  Job result{};
  while (ptr < endPtr) {
    auto typed = (ajm::OpcodeHeader *)ptr;
    switch (typed->getOpcode()) {
    case ajm::Opcode::ReturnAddress: {
      // ReturnAddress *ra = (ReturnAddress *)jobPtr;
      // ORBIS_LOG_ERROR(__FUNCTION__, request, "return address",
      // ra->opcode,
      //                 ra->unk, ra->returnAddress);
      ptr += sizeof(ajm::ReturnAddress);
      break;
    }
    case ajm::Opcode::ControlBufferRa: {
      auto *ctrl = (ajm::BatchJobControlBufferRa *)ptr;
      auto *sidebandResult =
          reinterpret_cast<ajm::SidebandResult *>(ctrl->pSidebandOutput);
      *sidebandResult = {};
      result.pSidebandResult = sidebandResult;
      result.controlFlags = ctrl->getFlags();
      ptr += sizeof(ajm::BatchJobControlBufferRa);
      break;
    }
    case ajm::Opcode::RunBufferRa: {
      auto *job = (ajm::BatchJobInputBufferRa *)ptr;
      // ORBIS_LOG_ERROR(__FUNCTION__, request, "BatchJobInputBufferRa",
      //                 job->opcode, job->szInputSize, job->pInput);

      // rx::hexdump({(std::byte*) job->pInput, job->szInputSize});
      result.inputBuffers.push_back({job->pInput, job->szInputSize});
      ptr += sizeof(ajm::BatchJobInputBufferRa);
      break;
    }
    case ajm::Opcode::Flags: {
      auto *job = (ajm::BatchJobFlagsRa *)ptr;
      // ORBIS_LOG_ERROR(__FUNCTION__, request, "BatchJobFlagsRa",
      //                 job->flagsHi, job->flagsLo);
      result.flags = ((orbis::uint64_t)job->flagsHi << 0x1a) | job->flagsLo;
      ptr += sizeof(ajm::BatchJobFlagsRa);
      break;
    }
    case ajm::Opcode::JobBufferOutputRa: {
      auto *job = (ajm::BatchJobOutputBufferRa *)ptr;
      // ORBIS_LOG_ERROR(__FUNCTION__, request, "BatchJobOutputBufferRa",
      //                 job->opcode, job->outputSize, job->pOutput);
      result.outputBuffers.push_back({job->pOutput, job->outputSize});
      result.totalOutputSize += job->outputSize;
      ptr += sizeof(ajm::BatchJobOutputBufferRa);
      break;
    }
    case ajm::Opcode::JobBufferSidebandRa: {
      auto *job = (ajm::BatchJobSidebandBufferRa *)ptr;
      // ORBIS_LOG_ERROR(__FUNCTION__, request, "BatchJobSidebandBufferRa",
      //                 job->opcode, job->sidebandSize, job->pSideband);
      result.pSideband = job->pSideband;
      result.sidebandSize = job->sidebandSize;
      ptr += sizeof(ajm::BatchJobSidebandBufferRa);
      break;
    }
    default:
      ORBIS_LOG_ERROR(__FUNCTION__, "unexpected job opcode",
                      typed->getOpcode());
      ptr = endPtr;
      break;
    }
  }

  return result;
}
} // namespace ajm

struct AjmFile : orbis::File {};

enum AjmIoctlRequest {
  AJM_IOCTL_CONTEXT_UNREGISTER = 0xc0288900,
  AJM_IOCTL_MODULE_REGISTER = 0xc0288903,
  AJM_IOCTL_MODULE_UNREGISTER = 0xc0288904,
  AJM_IOCTL_INSTANCE_CREATE = 0xc0288905,
  AJM_IOCTL_INSTANCE_DESTROY = 0xc0288906,
  AJM_IOCTL_INSTANCE_EXTEND = 0xc028890a,
  AJM_IOCTL_INSTANCE_SWITCH = 0xc028890b,
  AJM_IOCTL_BATCH_RUN = 0xc0288907,
  AJM_IOCTL_BATCH_WAIT = 0xc0288908,
};

static orbis::ErrorCode ajm_ioctl(orbis::File *file, std::uint64_t request,
                                  void *argp, orbis::Thread *thread) {

  auto device = static_cast<AjmDevice *>(file->device.get());

  switch (AjmIoctlRequest(request)) {
  case AJM_IOCTL_CONTEXT_UNREGISTER:
  case AJM_IOCTL_MODULE_REGISTER:
  case AJM_IOCTL_MODULE_UNREGISTER: {
    struct Args {
      orbis::uint32_t result;
    };

    auto args = reinterpret_cast<Args *>(argp);
    ORBIS_LOG_ERROR(__FUNCTION__, request, args);
    args->result = 0;
    return {};
  }

  case AJM_IOCTL_INSTANCE_CREATE: {
    struct InstanceCreateArgs {
      orbis::uint32_t result;
      orbis::uint32_t unk0;
      orbis::uint64_t flags;
      orbis::uint32_t codec;
      ajm::PackedInstanceId instanceId;
    };

    auto args = reinterpret_cast<InstanceCreateArgs *>(argp);
    auto codecId = ajm::CodecId(args->codec);
    orbis::Ref<ajm::CodecInstance> instance;
    ORBIS_RET_ON_ERROR(
        device->createInstance(&instance, codecId, args->unk0, args->flags));

    auto instanceId = device->addCodecInstance(codecId, instance);
    args->result = 0;
    args->instanceId = ajm::PackedInstanceId::create(codecId, instanceId);
    return {};
  }

  case AJM_IOCTL_INSTANCE_DESTROY: {
    struct InstanceDestroyArgs {
      orbis::uint32_t result;
      orbis::uint32_t unk0;
      ajm::PackedInstanceId instanceId;
    };
    auto args = reinterpret_cast<InstanceDestroyArgs *>(argp);
    auto packedInstanceId = args->instanceId;
    ORBIS_RET_ON_ERROR(device->removeInstance(
        packedInstanceId.getCodecId(), packedInstanceId.getInstanceId()));
    args->result = 0;
    return {};
  }

  case AJM_IOCTL_INSTANCE_EXTEND:
    std::println(stderr, "ajm instance extend");
    std::abort();
    return {};

  case AJM_IOCTL_INSTANCE_SWITCH:
    std::println(stderr, "ajm instance switch");
    std::abort();
    return {};

  case AJM_IOCTL_BATCH_RUN: {
    struct BatchRunBufferArgs {
      orbis::uint32_t result;
      orbis::uint32_t unk0;
      std::byte *pBatch;
      orbis::uint32_t batchSize;
      orbis::uint32_t priority;
      orbis::uint64_t batchError;
      ajm::BatchId batchId;
    };

    auto args = reinterpret_cast<BatchRunBufferArgs *>(argp);
    // ORBIS_LOG_ERROR(__FUNCTION__, request, args->result, args->unk0,
    //                 args->pBatch, args->batchSize, args->priority,
    //                 args->batchError, args->batchId);
    // thread->where();

    auto ptr = args->pBatch;
    auto endPtr = args->pBatch + args->batchSize;

    std::map<orbis::Ref<ajm::CodecInstance>, std::vector<ajm::Job>> runJobMap;

    while (ptr < endPtr) {
      auto header = (ajm::InstructionHeader *)ptr;
      auto instanceId = ajm::PackedInstanceId{(header->id >> 6) & 0xfffff};
      auto jobPtr = ptr + sizeof(ajm::InstructionHeader);
      auto endJobPtr = ptr + header->len;
      // TODO: handle unimplemented codecs, so auto create instance for now
      auto runJob = ajm::decodeJob(jobPtr, endJobPtr);
      ptr = endJobPtr;

      auto instance = device->getInstance(instanceId.getCodecId(),
                                          instanceId.getInstanceId());

      if (instance == nullptr) {
        return orbis::ErrorCode::BADF;
      }

      auto [it, inserted] = runJobMap.try_emplace(std::move(instance));
      it->second.push_back(std::move(runJob));
    }

    auto batch = orbis::knew<ajm::Batch>();

    for (auto &[instance, runJobs] : runJobMap) {
      instance->runBatch(batch, std::move(runJobs));
    }

    args->result = 0;
    args->batchId = device->addBatch(batch);
    return {};
  }
  case AJM_IOCTL_BATCH_WAIT: {
    struct Args {
      orbis::uint32_t result;
      orbis::uint32_t unk1;
      ajm::BatchId batchId;
      orbis::uint32_t timeout;
      orbis::uint64_t batchError;
    };
    auto args = reinterpret_cast<Args *>(argp);
    auto batch = device->getBatch(args->batchId);
    if (batch == nullptr) {
      return orbis::ErrorCode::BADF;
    }

    // ORBIS_LOG_ERROR(__FUNCTION__, request, args->unk0, args->unk1,
    //                 args->batchId, args->timeout, args->batchError);
    // thread->where();

    std::uint64_t batchError = 0;
    ORBIS_RET_ON_ERROR(batch->wait(args->timeout, &batchError));
    args->result = 0;
    args->batchError = batchError;

    return {};
  }
  }

  ORBIS_LOG_FATAL("Unhandled AJM ioctl", request);
  thread->where();
  return {};
}

static const orbis::FileOps fileOps = {
    .ioctl = ajm_ioctl,
};

orbis::ErrorCode AjmDevice::open(orbis::Ref<orbis::File> *file,
                                 const char *path, std::uint32_t flags,
                                 std::uint32_t mode, orbis::Thread *thread) {
  auto newFile = orbis::knew<AjmFile>();
  newFile->ops = &fileOps;
  newFile->device = this;

  *file = newFile;
  return {};
}

IoDevice *createAjmCharacterDevice() { return orbis::knew<AjmDevice>(); }
