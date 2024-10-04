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
  AlsaDevice();
  ~AlsaDevice() override;

  void init() override {};
  void start() override;
  long write(void *, long) override;
  void stop() override;
  void reset() override;

  void setAlsaFormat();

  int fixXRun();
  int resumeFromSupsend();

  audio_buf_info getOSpace() override;
};
