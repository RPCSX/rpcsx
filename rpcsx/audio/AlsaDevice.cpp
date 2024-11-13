#include "AlsaDevice.hpp"
#include "orbis/utils/Logs.hpp"
#include "rx/hexdump.hpp"

void AlsaDevice::start() {
  if (mWorking) {
    // FIXME: should probably return error in this case and in other places
    return;
  }

  {
    _snd_pcm_format fmt;
    switch (mFormat) {
    case AudioFormat::S32_LE:
      fmt = SND_PCM_FORMAT_S32_LE;
      break;
    case AudioFormat::S16_LE:
      fmt = SND_PCM_FORMAT_S16_LE;
      break;
    case AudioFormat::AC3:
    default:
      ORBIS_LOG_FATAL("Format is not supported", int(mFormat));
      std::abort();
      break;
    }
    mAlsaFormat = fmt;
  }

  if (auto err =
          snd_pcm_open(&mPCMHandle, "default", SND_PCM_STREAM_PLAYBACK, 0);
      err < 0) {
    ORBIS_LOG_FATAL("Cannot open audio device", snd_strerror(err));
    std::abort();
  }

  if (auto err = snd_pcm_hw_params_malloc(&mHWParams); err < 0) {
    ORBIS_LOG_FATAL("Cannot allocate hardware parameter structure",
                    snd_strerror(err));
    std::abort();
  }

  if (auto err = snd_pcm_hw_params_any(mPCMHandle, mHWParams); err < 0) {
    ORBIS_LOG_FATAL("Cannot initialize hardware parameter structure",
                    snd_strerror(err));
    std::abort();
  }

  if (auto err = snd_pcm_hw_params_set_rate_resample(mPCMHandle, mHWParams, 0);
      err < 0) {
    ORBIS_LOG_FATAL("Cannot disable rate resampling", snd_strerror(err));
    std::abort();
  }

  if (auto err = snd_pcm_hw_params_set_access(mPCMHandle, mHWParams,
                                              SND_PCM_ACCESS_RW_INTERLEAVED);
      err < 0) {
    ORBIS_LOG_FATAL("Cannot set access type", snd_strerror(err));
    std::abort();
  }
  if (auto err =
          snd_pcm_hw_params_set_format(mPCMHandle, mHWParams, mAlsaFormat);
      err < 0) {
    ORBIS_LOG_FATAL("Cannot set sample format", snd_strerror(err));
    std::abort();
  }

  if (auto err =
          snd_pcm_hw_params_set_rate(mPCMHandle, mHWParams, mFrequency, 0);
      err < 0) {
    ORBIS_LOG_FATAL("Cannot set sample rate", snd_strerror(err));
    std::abort();
  }

  if (auto err =
          snd_pcm_hw_params_set_channels(mPCMHandle, mHWParams, mChannels);
      err < 0) {
    ORBIS_LOG_FATAL("cannot set channel count", snd_strerror(err), mChannels);
    std::abort();
  }

  if (auto err = snd_pcm_hw_params_set_periods(mPCMHandle, mHWParams,
                                               mSampleCount * 10, 0);
      err < 0) {
    ORBIS_LOG_FATAL("Cannot set periods count", snd_strerror(err));
    std::abort();
  }

  int frameBytes = snd_pcm_format_physical_width(mAlsaFormat) * mChannels / 8;

  snd_pcm_uframes_t size = mSampleSize / frameBytes;

  {
    auto trySize = size * mSampleCount * 8;
    int err = -1;
    while (trySize >= 1024) {
      err = snd_pcm_hw_params_set_buffer_size(mPCMHandle, mHWParams, trySize);
      if (err < 0) {
        trySize /= 2;
        continue;
      }

      break;
    }

    if (err < 0) {
      trySize = size * mSampleCount * 8;
      err = snd_pcm_hw_params_set_buffer_size_near(mPCMHandle, mHWParams,
                                                   &trySize);
    }

    if (err < 0) {
      ORBIS_LOG_FATAL("Cannot set buffer size", snd_strerror(err));
      std::abort();
    }
  }

  {
    auto trySize = size * mSampleCount;
    int err = -1;
    while (trySize >= 256) {
      err =
          snd_pcm_hw_params_set_period_size(mPCMHandle, mHWParams, trySize, 0);
      if (err < 0) {
        trySize /= 2;
        continue;
      }

      break;
    }

    if (err < 0) {
      trySize = size * mSampleCount;
      err = snd_pcm_hw_params_set_period_size_near(mPCMHandle, mHWParams,
                                                   &trySize, 0);
    }

    if (err < 0) {
      ORBIS_LOG_FATAL("Cannot set period size", snd_strerror(err));
      std::abort();
    }
  }

  snd_pcm_uframes_t periodSize;
  if (auto err =
          snd_pcm_hw_params_get_period_size(mHWParams, &periodSize, nullptr);
      err < 0) {
    ORBIS_LOG_FATAL("cannot set parameters", snd_strerror(err));
    std::abort();
  }

  snd_pcm_uframes_t bufferSize;
  if (auto err = snd_pcm_hw_params_get_buffer_size(mHWParams, &bufferSize);
      err < 0) {
    ORBIS_LOG_FATAL("cannot set parameters", snd_strerror(err));
    std::abort();
  }

  ORBIS_LOG_TODO("period and buffer", periodSize, bufferSize);

  if (auto err = snd_pcm_hw_params(mPCMHandle, mHWParams); err < 0) {
    ORBIS_LOG_FATAL("cannot set parameters", snd_strerror(err));
    std::abort();
  }

  if (auto err = snd_pcm_sw_params_malloc(&mSWParams); err < 0) {
    ORBIS_LOG_FATAL("Cannot allocate software parameter structure",
                    snd_strerror(err));
    std::abort();
  }

  if (auto err = snd_pcm_sw_params_current(mPCMHandle, mSWParams); err < 0) {
    ORBIS_LOG_FATAL("cannot sw params current", snd_strerror(err));
    std::abort();
  }

  if (auto err = snd_pcm_sw_params_set_start_threshold(mPCMHandle, mSWParams,
                                                       periodSize);
      err < 0) {
    ORBIS_LOG_FATAL("cannot set start threshold", snd_strerror(err));
    std::abort();
  }

  if (auto err = snd_pcm_sw_params_set_stop_threshold(mPCMHandle, mSWParams,
                                                      bufferSize);
      err < 0) {
    ORBIS_LOG_FATAL("cannot set stop threshold", snd_strerror(err));
    std::abort();
  }

  if (auto err = snd_pcm_sw_params(mPCMHandle, mSWParams); err < 0) {
    ORBIS_LOG_FATAL("cannot set parameters", snd_strerror(err));
    std::abort();
  }

  if (auto err = snd_pcm_nonblock(mPCMHandle, 1); err < 0) {
    ORBIS_LOG_FATAL("set nonblock mode failed", snd_strerror(err));
    std::abort();
  }

  if (auto err = snd_pcm_prepare(mPCMHandle); err < 0) {
    ORBIS_LOG_FATAL("cannot prepare audio interface for use",
                    snd_strerror(err));
    std::abort();
  }

  mWorking = true;
}

int AlsaDevice::fixXRun() {
  switch (snd_pcm_state(mPCMHandle)) {
  case SND_PCM_STATE_XRUN:
    return snd_pcm_prepare(mPCMHandle);

  case SND_PCM_STATE_DRAINING:
    if (snd_pcm_stream(mPCMHandle) == SND_PCM_STREAM_CAPTURE)
      return snd_pcm_prepare(mPCMHandle);
    break;

  default:
    break;
  }

  return -EIO;
}

int AlsaDevice::resume() {
  if (int err = snd_pcm_resume(mPCMHandle); err == -EAGAIN) {
    return err;
  }

  return snd_pcm_prepare(mPCMHandle);
}

long AlsaDevice::write(void *buf, long len) {
  if (!mWorking)
    return 0;

  int frameBytes = snd_pcm_format_physical_width(mAlsaFormat) * mChannels / 8;
  snd_pcm_uframes_t frames = len / frameBytes;
  if (frames == 0) {
    return 0;
  }

  while (true) {
    snd_pcm_sframes_t r = snd_pcm_writei(mPCMHandle, buf, frames);

    if (r == -EPIPE) {
      r = fixXRun();
    }

    if (r == -ESTRPIPE) {
      r = resume();
    }

    if (r == 0 || r == -EAGAIN) {
      continue;
    }

    if (r < 0) {
      ORBIS_LOG_ERROR(__PRETTY_FUNCTION__, snd_strerror(r));
      return r;
    }

    r *= frameBytes;
    return r;
  }
}

void AlsaDevice::stop() {
  if (!mWorking) {
    return;
  }

  snd_pcm_hw_params_free(mHWParams);
  snd_pcm_sw_params_free(mSWParams);
  snd_pcm_drain(mPCMHandle);
  snd_pcm_drop(mPCMHandle);
  mWorking = false;
}

void AlsaDevice::reset() {
  if (!mWorking)
    return;
  int err = snd_pcm_drop(mPCMHandle);

  if (err >= 0) {
    err = snd_pcm_prepare(mPCMHandle);
  }

  if (err < 0) {
    ORBIS_LOG_ERROR(__PRETTY_FUNCTION__, snd_strerror(err));
  }
}

audio_buf_info AlsaDevice::getOSpace() {
  int err;
  snd_pcm_uframes_t periodSize;
  if ((err = snd_pcm_hw_params_get_period_size(mHWParams, &periodSize, NULL)) <
      0) {
    ORBIS_LOG_FATAL("cannot get period size", snd_strerror(err));
    std::abort();
  }

  snd_pcm_uframes_t bufferSize;
  if ((err = snd_pcm_hw_params_get_buffer_size(mHWParams, &bufferSize)) < 0) {
    ORBIS_LOG_FATAL("cannot get buffer size", snd_strerror(err));
    std::abort();
  }
  int frameBytes = snd_pcm_format_physical_width(mAlsaFormat) * mChannels / 8;

  snd_pcm_sframes_t avail = snd_pcm_avail_update(mPCMHandle);
  if (avail < 0 || (snd_pcm_uframes_t)avail > bufferSize)
    avail = bufferSize;

  return {
      .fragments = static_cast<orbis::sint>(avail / periodSize),
      .fragsize = static_cast<orbis::sint>(mSampleCount),
      .fragstotal = static_cast<orbis::sint>(periodSize * frameBytes),
      .bytes = static_cast<orbis::sint>(avail * frameBytes),
  };
}
