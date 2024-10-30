#pragma once

#include "libatrac9/libatrac9.h"
#include "orbis-config.hpp"
// #include "orbis/utils/Logs.hpp"
#include <cstdint>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
}

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

  Opcode getOpcode() const {
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
  std::byte *pSidebandInput;
  orbis::uint32_t flagsHi;
  orbis::uint32_t flagsLo;
  orbis::uint32_t commandId;
  orbis::uint32_t sidebandOutputSize;
  std::byte *pSidebandOutput;

  std::uint64_t getFlags() { return ((uint64_t)flagsHi << 0x1a) | flagsLo; }
};
static_assert(sizeof(BatchJobControlBufferRa) == 0x28);

struct BatchJobInputBufferRa {
  orbis::uint32_t opcode;
  orbis::uint32_t szInputSize;
  std::byte *pInput;
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
  std::byte *pOutput;
};
static_assert(sizeof(BatchJobOutputBufferRa) == 0x10);

struct BatchJobSidebandBufferRa {
  orbis::uint32_t opcode;
  orbis::uint32_t sidebandSize;
  std::byte *pSideband;
};
static_assert(sizeof(BatchJobSidebandBufferRa) == 0x10);

struct RunJob {
  orbis::uint64_t flags;
  orbis::uint32_t outputSize;
  std::byte *pOutput;
  orbis::uint32_t sidebandSize;
  std::byte *pSideband;
  bool control;
};

// Thanks to mystical SirNickity with 1 post
// https://hydrogenaud.io/index.php?topic=85125.msg747716#msg747716

const uint8_t mpeg_versions[4] = {25, 0, 2, 1};

// Layers - use [layer]
const uint8_t mpeg_layers[4] = {0, 3, 2, 1};

// Bitrates - use [version][layer][bitrate]
const uint16_t mpeg_bitrates[4][4][16] = {
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
const uint16_t mpeg_srates[4][4] = {
    {11025, 12000, 8000, 0},  // MPEG 2.5
    {0, 0, 0, 0},             // Reserved
    {22050, 24000, 16000, 0}, // MPEG 2
    {44100, 48000, 32000, 0}  // MPEG 1
};

// Samples per frame - use [version][layer]
const uint16_t mpeg_frame_samples[4][4] = {
    //    Rsvd     3     2     1  < Layer  v Version
    {0, 576, 1152, 384}, //       2.5
    {0, 0, 0, 0},        //       Reserved
    {0, 576, 1152, 384}, //       2
    {0, 1152, 1152, 384} //       1
};

// Slot size (MPEG unit of measurement) - use [layer]
const uint8_t mpeg_slot_size[4] = {0, 1, 1, 4}; // Rsvd, 3, 2, 1

uint32_t get_mp3_data_size(const uint8_t *data) {
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
  //                (uint16_t)pad, (uint16_t)brx, (uint16_t)srx, bitrate, samprate,
  //                samples, (uint16_t)slot_size, bps, fsize,
  //                static_cast<uint16_t>(fsize));

  // Frame sizes are truncated integers
  return static_cast<uint16_t>(fsize);
}

enum AACHeaderType { AAC_ADTS = 1, AAC_RAW = 2 };

orbis::uint32_t AACFreq[12] = {96000, 88200, 64000, 48000, 44100, 32000,
                               24000, 22050, 16000, 12000, 11025, 8000};

enum AJMCodecs : orbis::uint32_t {
  AJM_CODEC_MP3 = 0,
  AJM_CODEC_At9 = 1,
  AJM_CODEC_AAC = 2,
};

enum AJMChannels : orbis::uint32_t {
  AJM_CHANNEL_DEFAULT = 0,
  AJM_CHANNEL_1 = 1,
  AJM_CHANNEL_2 = 2,
  AJM_CHANNEL_3 = 3,
  AJM_CHANNEL_4 = 4,
  AJM_CHANNEL_5 = 5,
  AJM_CHANNEL_6 = 6,
  AJM_CHANNEL_8 = 8,
};

enum AJMFormat : orbis::uint32_t {
  AJM_FORMAT_S16 = 0, // default
  AJM_FORMAT_S32 = 1,
  AJM_FORMAT_FLOAT = 2
};

struct At9Instance {
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
  orbis::uint32_t configData;
};

struct AACInstance {
  AACHeaderType headerType;
  orbis::uint32_t sampleRate;
};

struct AJMSidebandGaplessDecode {
  orbis::uint32_t totalSamples;
  orbis::uint16_t skipSamples;
  orbis::uint16_t totalSkippedSamples;
};

struct AJMSidebandResult {
  orbis::int32_t result;
  orbis::int32_t codecResult;
};

struct AJMSidebandStream {
  orbis::int32_t inputSize;
  orbis::int32_t outputSize;
  orbis::uint64_t decodedSamples;
};

struct AJMSidebandMultipleFrames {
  orbis::uint32_t framesProcessed;
  orbis::uint32_t unk0;
};

struct AJMSidebandFormat {
  AJMChannels channels;
  orbis::uint32_t unk0; // maybe channel mask?
  orbis::uint32_t sampleRate;
  AJMFormat sampleFormat;
  uint32_t bitrate;
  uint32_t unk1;
};

struct AJMAt9CodecInfoSideband {
  orbis::uint32_t superFrameSize;
  orbis::uint32_t framesInSuperFrame;
  orbis::uint32_t unk0;
  orbis::uint32_t frameSamples;
};

struct AJMMP3CodecInfoSideband {
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

struct AJMAACCodecInfoSideband {
  orbis::uint32_t heaac;
  orbis::uint32_t unk0;
};

struct Instance {
  AJMCodecs codec;
  AJMChannels maxChannels;
  AJMFormat outputFormat;
  At9Instance at9;
  AACInstance aac;
  AVCodecContext *codecCtx;
  SwrContext *resampler;
  orbis::uint32_t lastBatchId;
  // TODO: use AJMSidebandGaplessDecode for these variables
  AJMSidebandGaplessDecode gapless;
  orbis::uint32_t processedSamples;
  AJMSidebandFormat lastDecode;
};

enum ControlFlags {
  CONTROL_INITIALIZE = 0x4000,
  CONTROL_RESET = 0x2000,
};

enum RunFlags {
  RUN_MULTIPLE_FRAMES = 0x1000,
  RUN_GET_CODEC_INFO = 0x800,
};

enum SidebandFlags {
  SIDEBAND_STREAM = 0x800000000000,
  SIDEBAND_FORMAT = 0x400000000000,
  SIDEBAND_GAPLESS_DECODE = 0x200000000000,
};