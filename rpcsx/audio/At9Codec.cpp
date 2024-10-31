#include "../iodev/ajm.hpp"
#include <cstddef>

extern "C" {
#include <libatrac9/decoder.h>
#include <libatrac9/libatrac9.h>
}

struct At9CodecInfoSideband {
  orbis::uint32_t superFrameSize;
  orbis::uint32_t framesInSuperFrame;
  orbis::uint32_t unk0;
  orbis::uint32_t frameSamples;
};

struct At9CodecInstance : ajm::CodecInstance {
  std::vector<std::byte> inputBuffer;
  std::vector<std::byte> outputBuffer;

  orbis::ptr<void> handle{};
  orbis::uint32_t inputChannels{};
  orbis::uint32_t framesInSuperframe{};
  orbis::uint32_t frameSamples{};
  orbis::uint32_t superFrameDataLeft{};
  orbis::uint32_t superFrameDataIdx{};
  orbis::uint32_t superFrameSize{};
  orbis::uint32_t estimatedSizeUsed{};
  orbis::uint32_t sampleRate{};
  Atrac9Format outputFormat{};
  orbis::uint32_t configData{};

  ~At9CodecInstance() {
    if (handle) {
      Atrac9ReleaseHandle(handle);
    }
  }

  std::uint64_t runJob(const ajm::Job &job) override {}
  void reset() override {}
};

struct At9Codec : ajm::Codec {
  orbis::ErrorCode createInstance(orbis::Ref<ajm::CodecInstance> *instance,
                                  std::uint32_t unk0,
                                  std::uint64_t flags) override {
    auto at9 = orbis::Ref{orbis::knew<At9CodecInstance>()};
    *instance = at9;
    return {};
  }
};

void createAt9Codec(AjmDevice *ajm) {
  ajm->createCodec<At9Codec>(ajm::CodecId::At9);
}
