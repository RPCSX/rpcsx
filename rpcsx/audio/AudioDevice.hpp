#pragma once

#include "orbis-config.hpp"
#include "rx/Rc.hpp"
#include <cstdlib>
#include <orbis/sys/sysproto.hpp>

struct [[gnu::packed]] audio_buf_info {
  orbis::sint fragments;
  orbis::sint fragsize;
  orbis::sint fragstotal;
  orbis::sint bytes;
};

enum class AudioFormat : std::uint32_t {
  S16_LE = 0x10,
  AC3 = 0x400,
  S32_LE = 0x1000,
};

class AudioDevice : public rx::RcBase {
protected:
  bool mWorking = false;
  AudioFormat mFormat{};
  orbis::uint mFrequency{};
  orbis::ushort mChannels{};
  orbis::ushort mSampleSize{};
  orbis::ushort mSampleCount{};

public:
  virtual ~AudioDevice() = default;

  virtual void init() {}
  virtual void start() {}
  virtual long write(void *buf, long len) { return -1; }
  virtual void stop() {}
  virtual void reset() {}

  void setFormat(AudioFormat format);
  void setFrequency(orbis::uint frequency);
  void setChannels(orbis::ushort channels);
  void setSampleSize(orbis::uint sampleSize = 0, orbis::uint sampleCount = 0);

  virtual audio_buf_info getOSpace() { return {}; }
};
