#include "../iodev/ajm.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavcodec/codec_internal.h>
#include <libavcodec/packet.h>
#include <libavformat/avformat.h>
#include <libavutil/mem.h>
#include <libavutil/opt.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
}

// Thanks to mystical SirNickity with 1 post
// https://hydrogenaud.io/index.php?topic=85125.msg747716#msg747716

inline constexpr uint8_t mpeg_versions[4] = {25, 0, 2, 1};

// Layers - use [layer]
inline constexpr uint8_t mpeg_layers[4] = {0, 3, 2, 1};

// Bitrates - use [version][layer][bitrate]
inline constexpr uint16_t mpeg_bitrates[4][4][16] = {
    {
        // Version 2.5
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // Reserved
        {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160,
         0}, // Layer 3
        {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160,
         0}, // Layer 2
        {0, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256,
         0} // Layer 1
    },
    {
        // Reserved
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // Invalid
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // Invalid
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // Invalid
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}  // Invalid
    },
    {
        // Version 2
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // Reserved
        {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160,
         0}, // Layer 3
        {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160,
         0}, // Layer 2
        {0, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256,
         0} // Layer 1
    },
    {
        // Version 1
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // Reserved
        {0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320,
         0}, // Layer 3
        {0, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384,
         0}, // Layer 2
        {0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448,
         0}, // Layer 1
    }};

// Sample rates - use [version][srate]
inline constexpr uint16_t mpeg_srates[4][4] = {
    {11025, 12000, 8000, 0},  // MPEG 2.5
    {0, 0, 0, 0},             // Reserved
    {22050, 24000, 16000, 0}, // MPEG 2
    {44100, 48000, 32000, 0}  // MPEG 1
};

// Samples per frame - use [version][layer]
inline constexpr uint16_t mpeg_frame_samples[4][4] = {
    //    Rsvd     3     2     1  < Layer  v Version
    {0, 576, 1152, 384}, //       2.5
    {0, 0, 0, 0},        //       Reserved
    {0, 576, 1152, 384}, //       2
    {0, 1152, 1152, 384} //       1
};

// Slot size (MPEG unit of measurement) - use [layer]
inline constexpr uint8_t mpeg_slot_size[4] = {0, 1, 1, 4}; // Rsvd, 3, 2, 1

constexpr uint32_t get_mp3_data_size(const uint8_t *data) {
  // Quick validity check
  if (((data[0] & 0xFF) != 0xFF) || ((data[1] & 0xE0) != 0xE0) // 3 sync bits
      || ((data[1] & 0x18) == 0x08)                            // Version rsvd
      || ((data[1] & 0x06) == 0x00)                            // Layer rsvd
      || ((data[2] & 0xF0) == 0xF0)                            // Bitrate rsvd
  ) {
    return 0;
  }

  // Data to be extracted from the header
  uint8_t ver = (data[1] & 0x18) >> 3; // Version index
  uint8_t lyr = (data[1] & 0x06) >> 1; // Layer index
  uint8_t pad = (data[2] & 0x02) >> 1; // Padding? 0/1
  uint8_t brx = (data[2] & 0xf0) >> 4; // Bitrate index
  uint8_t srx = (data[2] & 0x0c) >> 2; // SampRate index

  // Lookup real values of these fields
  uint32_t bitrate = mpeg_bitrates[ver][lyr][brx] * 1000;
  uint32_t samprate = mpeg_srates[ver][srx];
  uint16_t samples = mpeg_frame_samples[ver][lyr];
  uint8_t slot_size = mpeg_slot_size[lyr];

  // In-between calculations
  float bps = static_cast<float>(samples) / 8.0f;
  float fsize =
      ((bps * static_cast<float>(bitrate)) / static_cast<float>(samprate)) +
      ((pad) ? slot_size : 0);

  // ORBIS_LOG_TODO(__FUNCTION__, (uint16_t)ver, (uint16_t)lyr,
  //                (uint16_t)pad, (uint16_t)brx, (uint16_t)srx, bitrate,
  //                samprate, samples, (uint16_t)slot_size, bps, fsize,
  //                static_cast<uint16_t>(fsize));

  // Frame sizes are truncated integers
  return static_cast<uint16_t>(fsize);
}

inline constexpr orbis::uint32_t kAACFreq[12] = {96000, 88200, 64000, 48000,
                                                 44100, 32000, 24000, 22050,
                                                 16000, 12000, 11025, 8000};

struct MP3CodecInfoSideband {
  orbis::uint32_t header;
  orbis::uint8_t unk0;
  orbis::uint8_t unk1;
  orbis::uint8_t unk2;
  orbis::uint8_t unk3;
  orbis::uint8_t unk4;
  orbis::uint8_t unk5;
  orbis::uint16_t unk6;
  orbis::uint16_t unk7;
  orbis::uint16_t unk8;
};

struct AACCodecInfoSideband {
  orbis::uint32_t heaac;
  orbis::uint32_t unk0;
};

enum AACHeaderType { AAC_ADTS = 1, AAC_RAW = 2 };

static AVSampleFormat ajmToAvFormat(ajm::Format ajmFormat) {
  switch (ajmFormat) {
  case ajm::Format::S16:
    return AV_SAMPLE_FMT_S16;
  case ajm::Format::S32:
    return AV_SAMPLE_FMT_S32;
  case ajm::Format::Float:
    return AV_SAMPLE_FMT_FLTP;
  }

  return AV_SAMPLE_FMT_NONE;
}

struct FFmpegCodecInstance : ajm::CodecInstance {
  std::uint64_t runJob(const ajm::Job &job) override { return 0; }
  void reset() override {}
};

struct AACInstance {
  AACHeaderType headerType;
  orbis::uint32_t sampleRate;
};

// struct Instance {
//   orbis::shared_mutex mtx;
//   CodecId codecId;
//   ChannelCount maxChannels;
//   Format outputFormat;
//   At9Instance at9;
//   AACInstance aac;
//   orbis::kvector<std::byte> inputBuffer;
//   orbis::kvector<std::byte> outputBuffer;

//   AVCodecContext *codecCtx;
//   SwrContext *resampler;
//   orbis::uint32_t processedSamples;
//   SidebandGaplessDecode gapless;
//   SidebandFormat lastDecode;
// };

struct MP3FFmpegCodecInstance : FFmpegCodecInstance {
  std::uint64_t runJob(const ajm::Job &job) override { return 0; }
  void reset() override {}
};
struct AACFFmpegCodecInstance : FFmpegCodecInstance {
  std::uint64_t runJob(const ajm::Job &job) override { return 0; }
  void reset() override {}
};

struct MP3FFmpegCodec : ajm::Codec {
  orbis::ErrorCode createInstance(orbis::Ref<ajm::CodecInstance> *instance,
                                  std::uint32_t unk0,
                                  std::uint64_t flags) override {
    auto mp3 = orbis::Ref{orbis::knew<MP3FFmpegCodecInstance>()};
    *instance = mp3;
    return {};
  }
};

struct AACFFmpegCodec : ajm::Codec {
  orbis::ErrorCode createInstance(orbis::Ref<ajm::CodecInstance> *instance,
                                  std::uint32_t unk0,
                                  std::uint64_t flags) override {
    auto aac = orbis::Ref{orbis::knew<AACFFmpegCodecInstance>()};
    *instance = aac;
    return {};
  }
};

void createFFmpegCodecs(AjmDevice *ajm) {
  ajm->createCodec<AACFFmpegCodec>(ajm::CodecId::AAC);
  ajm->createCodec<MP3FFmpegCodec>(ajm::CodecId::MP3);
}
