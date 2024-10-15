#include "AudioDevice.hpp"
#include "orbis/utils/Logs.hpp"

void AudioDevice::setFormat(AudioFormat format) {
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

void AudioDevice::setSampleSize(orbis::uint sampleSize,
                                orbis::uint sampleCount) {
  if (mWorking)
    return;
  mSampleSize = sampleSize;
  mSampleCount = sampleCount;
}
