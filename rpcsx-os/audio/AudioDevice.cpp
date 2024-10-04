#include "AudioDevice.hpp"
#include "orbis/utils/Logs.hpp"
#include "rx/hexdump.hpp"

AudioDevice::AudioDevice() {}

void AudioDevice::init() {}

void AudioDevice::start() {}

long AudioDevice::write(void *buf, long len) {
  return -1;
}

void AudioDevice::stop() {
}

void AudioDevice::reset() {}

void AudioDevice::setFormat(orbis::uint format) {
  if (mWorking)
    return;
  mFormat = format;
}

void AudioDevice::setFrequency(orbis::uint frequency) {
  if (mWorking)
    return;
  mFrequency = frequency;
}

void AudioDevice::setChannels(orbis::ushort channels) {
  if (mWorking)
    return;
  if (channels > 8) {
    ORBIS_LOG_FATAL("Channels count is not supported", channels);
    std::abort();
  }
  mChannels = channels;
}

void AudioDevice::setSampleSize(orbis::uint sampleSize, orbis::uint sampleCount) {
  if (mWorking)
    return;
  mSampleSize = sampleSize;
  mSampleCount = sampleCount;
}

audio_buf_info AudioDevice::getOSpace() {
  audio_buf_info info;
  return info;
}


AudioDevice::~AudioDevice() {}