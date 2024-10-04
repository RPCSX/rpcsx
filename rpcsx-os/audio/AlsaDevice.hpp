#pragma once

#include "AudioDevice.hpp"
#include <alsa/asoundlib.h>
#include <cstdlib>
#include <thread>

class AlsaDevice : public AudioDevice {
private:
  snd_pcm_format_t mAlsaFormat;
  snd_pcm_t *mPCMHandle;
  snd_pcm_hw_params_t *mHWParams;
  snd_pcm_sw_params_t *mSWParams;

public:
  ~AlsaDevice() { stop(); }

  void init() override {};
  void start() override;
  long write(void *, long) override;
  void stop() override;
  void reset() override;

  int fixXRun();
  int resume();

  audio_buf_info getOSpace() override;
};
