#pragma once

#define FMT_S16_LE  0x10
#define FMT_AC3     0x400
#define FMT_S32_LE  0x1000

#include <cstdlib>
#include <orbis/sys/sysproto.hpp>

struct audio_buf_info {
  int fragments;
  int fragstotal;
  int fragsize;
  int bytes;
};

class AudioDevice {
protected:
  bool mWorking = false;
  orbis::uint mFormat{};
  orbis::uint mFrequency{};
  orbis::ushort mChannels{};
  orbis::ushort mSampleSize{};
  orbis::ushort mSampleCount{};

private:

public:
  AudioDevice();
  virtual ~AudioDevice();

  virtual void init();
  virtual void start();
  virtual long write(void *buf, long len);
  virtual void stop();
  virtual void reset();

  void setFormat(orbis::uint format);
  void setFrequency(orbis::uint frequency);
  void setChannels(orbis::ushort channels);
  void setSampleSize(orbis::uint sampleSize = 0, orbis::uint sampleCount = 0);

  virtual audio_buf_info getOSpace();
};
