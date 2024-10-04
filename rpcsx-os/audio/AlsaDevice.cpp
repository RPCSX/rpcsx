#include "AlsaDevice.hpp"
#include "orbis/utils/Logs.hpp"
#include "rx/hexdump.hpp"

AlsaDevice::AlsaDevice() {}

void AlsaDevice::start() {
  setAlsaFormat();
  int err;
  if ((err = snd_pcm_open(&mPCMHandle, "default", SND_PCM_STREAM_PLAYBACK,
                          0)) < 0) {
    ORBIS_LOG_FATAL("Cannot open audio device", snd_strerror(err));
    std::abort();
  }

  if ((err = snd_pcm_hw_params_malloc(&mHWParams)) < 0) {
    ORBIS_LOG_FATAL("Cannot allocate hardware parameter structure",
                    snd_strerror(err));
    std::abort();
  }

  if ((err = snd_pcm_hw_params_any(mPCMHandle, mHWParams)) < 0) {
    ORBIS_LOG_FATAL("Cannot initialize hardware parameter structure",
                    snd_strerror(err));
    std::abort();
  }

  if ((err = snd_pcm_hw_params_set_rate_resample(mPCMHandle, mHWParams,
                                          0)) < 0) {
    ORBIS_LOG_FATAL("Cannot disable rate resampling", snd_strerror(err));
    std::abort();
  }

  if ((err = snd_pcm_hw_params_set_access(mPCMHandle, mHWParams,
                                          SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
    ORBIS_LOG_FATAL("Cannot set access type", snd_strerror(err));
    std::abort();
  }
  if ((err = snd_pcm_hw_params_set_format(mPCMHandle, mHWParams,
                                          mAlsaFormat)) < 0) {
    ORBIS_LOG_FATAL("Cannot set sample format", snd_strerror(err));
    std::abort();
  }

  if ((err = snd_pcm_hw_params_set_rate(mPCMHandle, mHWParams, mFrequency,
                                             0)) < 0) {
    ORBIS_LOG_FATAL("Cannot set sample rate", snd_strerror(err));
    std::abort();
  }

  if ((err = snd_pcm_hw_params_set_channels(mPCMHandle, mHWParams, mChannels)) <
      0) {
    ORBIS_LOG_FATAL("cannot set channel count", snd_strerror(err), mChannels);
    std::abort();
  }

  uint periods = mSampleCount;
  if ((err = snd_pcm_hw_params_set_periods_max(mPCMHandle, mHWParams, &periods, NULL)) < 0) {
    ORBIS_LOG_FATAL("Cannot set periods count", snd_strerror(err));
    std::abort();
  }

  int frameBytes = snd_pcm_format_physical_width(mAlsaFormat) * mChannels / 8;

  snd_pcm_uframes_t size = mSampleSize / frameBytes;

  // TODO: it shouldn't work like this

  if ((err = snd_pcm_hw_params_set_buffer_size(mPCMHandle, mHWParams, size)) < 0) {
    ORBIS_LOG_FATAL("Cannot set buffer size", snd_strerror(err));
    std::abort();
  }

  if ((err = snd_pcm_hw_params_set_period_size(mPCMHandle, mHWParams, size / 2, 0)) < 0) {
    ORBIS_LOG_FATAL("Cannot set period size", snd_strerror(err));
    std::abort();
  }

  snd_pcm_uframes_t periodSize;
  if ((err = snd_pcm_hw_params_get_period_size(mHWParams, &periodSize, NULL)) < 0) {
    ORBIS_LOG_FATAL("cannot set parameters", snd_strerror(err));
    std::abort();
  }

  snd_pcm_uframes_t bufferSize;
  if ((err = snd_pcm_hw_params_get_buffer_size(mHWParams, &bufferSize)) < 0) {
    ORBIS_LOG_FATAL("cannot set parameters", snd_strerror(err));
    std::abort();
  }

  ORBIS_LOG_TODO("period and buffer", periodSize, bufferSize);

  if ((err = snd_pcm_hw_params(mPCMHandle, mHWParams)) < 0) {
    ORBIS_LOG_FATAL("cannot set parameters", snd_strerror(err));
    std::abort();
  }

  if ((err = snd_pcm_sw_params_malloc(&mSWParams)) < 0) {
    ORBIS_LOG_FATAL("Cannot allocate software parameter structure",
                    snd_strerror(err));
    std::abort();
  }

  if ((err = snd_pcm_sw_params_current(mPCMHandle, mSWParams)) < 0) {
    ORBIS_LOG_FATAL("cannot sw params current", snd_strerror(err));
    std::abort();
  }

  if ((err = snd_pcm_sw_params_set_start_threshold(mPCMHandle, mSWParams, periodSize)) < 0) {
    ORBIS_LOG_FATAL("cannot set start threshold", snd_strerror(err));
    std::abort();
  }

  if ((err = snd_pcm_sw_params_set_stop_threshold(mPCMHandle, mSWParams, bufferSize)) < 0) {
    ORBIS_LOG_FATAL("cannot set stop threshold", snd_strerror(err));
    std::abort();
  }

  if ((err = snd_pcm_sw_params(mPCMHandle, mSWParams)) < 0) {
    ORBIS_LOG_FATAL("cannot set parameters", snd_strerror(err));
    std::abort();
  }

  if ((err = snd_pcm_prepare(mPCMHandle)) < 0) {
    ORBIS_LOG_FATAL("cannot prepare audio interface for use",
            snd_strerror(err));
    std::abort();
  }
  mWorking = true;
}

int AlsaDevice::fixXRun()
{
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

int AlsaDevice::resumeFromSupsend()
{
	int res;
	while ((res = snd_pcm_resume(mPCMHandle)) == -EAGAIN)
		std::this_thread::sleep_for(std::chrono::seconds(1));
	if (!res)
		return 0;
	return snd_pcm_prepare(mPCMHandle);
}

long AlsaDevice::write(void *buf, long len) {
  if (!mWorking) return 0;
  ssize_t r;
	int frameBytes = snd_pcm_format_physical_width(mAlsaFormat) * mChannels / 8;
  snd_pcm_uframes_t frames = len / frameBytes;

  r = snd_pcm_writei(mPCMHandle, buf, frames);
  if (r == -EPIPE) {
		if (!(r = fixXRun()))
			return write(buf, len);
	} else if (r == -ESTRPIPE) {
		if (!(r = resumeFromSupsend()))
			return write(buf, len);
	}
  r *= frameBytes;
	return r;
}

void AlsaDevice::stop() {
  snd_pcm_hw_params_free(mHWParams);
  snd_pcm_sw_params_free(mSWParams);
  snd_pcm_drain(mPCMHandle);
  snd_pcm_drop(mPCMHandle);
  mWorking = false;
}

void AlsaDevice::reset() {
  if (!mWorking) return;
  int err;
  err = snd_pcm_drop(mPCMHandle);
  if (err >= 0)
    err = snd_pcm_prepare(mPCMHandle);
  if (err < 0)
    err = err;
}

audio_buf_info AlsaDevice::getOSpace() {
  int err;
  snd_pcm_uframes_t periodSize;
  if ((err = snd_pcm_hw_params_get_period_size(mHWParams, &periodSize, NULL)) < 0) {
    ORBIS_LOG_FATAL("cannot get period size", snd_strerror(err));
    std::abort();
  }

  snd_pcm_uframes_t bufferSize;
  if ((err = snd_pcm_hw_params_get_buffer_size(mHWParams, &bufferSize)) < 0) {
    ORBIS_LOG_FATAL("cannot get buffer size", snd_strerror(err));
    std::abort();
  }
  int frameBytes = snd_pcm_format_physical_width(mAlsaFormat) * mChannels / 8;

  snd_pcm_sframes_t avail, delay;
  audio_buf_info info;
  avail = snd_pcm_avail_update(mPCMHandle);
  if (avail < 0 || (snd_pcm_uframes_t)avail > bufferSize)
    avail = bufferSize;
  info.fragsize = periodSize * frameBytes;
  info.fragstotal = mSampleCount;
  info.bytes = avail * frameBytes;
  info.fragments = avail / periodSize;
  return info;
}

void AlsaDevice::setAlsaFormat() {
  if (mWorking)
    return;
  _snd_pcm_format fmt;
  switch (mFormat) {
    case FMT_S32_LE:
      fmt = SND_PCM_FORMAT_S32_LE;
      break;
    case FMT_S16_LE:
      fmt = SND_PCM_FORMAT_S16_LE;
      break;
    case FMT_AC3:
    default:
      ORBIS_LOG_FATAL("Format is not supported", mFormat);
      std::abort();
      break;
  }
  mAlsaFormat = fmt;
}

AlsaDevice::~AlsaDevice() {
  stop();
}