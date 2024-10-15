#include "AudioOut.hpp"
#include "rx/mem.hpp"
#include "rx/watchdog.hpp"
#include <atomic>
#include <chrono>
#include <fcntl.h>
#include <format>
#include <mutex>
#include <orbis/evf.hpp>
#include <orbis/utils/Logs.hpp>
#include <sox.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <thread>
#include <vector>

AudioOut::AudioOut() {
  if (sox_init() != SOX_SUCCESS) {
    ORBIS_LOG_FATAL("Failed to initialize sox");
    std::abort();
  }
}

AudioOut::~AudioOut() {
  exit = true;
  for (auto &thread : threads) {
    thread.join();
  }
  sox_quit();
}

void AudioOut::start() {
  std::lock_guard lock(thrMtx);
  threads.emplace_back(
      [this, channelInfo = channelInfo] { channelEntry(channelInfo); });
}

void AudioOut::channelEntry(AudioOutChannelInfo info) {
  char control_shm_name[128];
  char audio_shm_name[128];

  std::format_to(
      control_shm_name, "{}",
      rx::getShmGuestPath(std::format("shm_{}_C", info.idControl)).string());
  std::format_to(
      audio_shm_name, "{}",
      rx::getShmGuestPath(std::format("shm_{}_{}_A", info.channel, info.port))
          .string());

  int controlFd = ::open(control_shm_name, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
  if (controlFd == -1) {
    perror("shm_open");
    std::abort();
  }

  struct stat controlStat;
  if (::fstat(controlFd, &controlStat)) {
    perror("fstat");
    std::abort();
  }

  auto controlPtr = reinterpret_cast<std::uint8_t *>(
      rx::mem::map(nullptr, controlStat.st_size, PROT_READ | PROT_WRITE,
                   MAP_SHARED, controlFd));
  if (controlPtr == MAP_FAILED) {
    perror("mmap");
    std::abort();
  }

  int bufferFd = ::open(audio_shm_name, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
  if (bufferFd == -1) {
    perror("open");
    std::abort();
  }

  struct stat bufferStat;
  if (::fstat(bufferFd, &bufferStat)) {
    perror("fstat");
    std::abort();
  }

  auto audioBuffer = ::mmap(NULL, bufferStat.st_size, PROT_READ | PROT_WRITE,
                            MAP_SHARED, bufferFd, 0);
  auto bitPattern = 1u << info.port;

  auto portOffset = 32 + 0x94 * info.port * 4;

  auto *params = reinterpret_cast<AudioOutParams *>(controlPtr + portOffset);

  // samples length will be inited after some time, so we wait for it
  while (params->sampleLength == 0) {
  }

  ORBIS_LOG_NOTICE("AudioOut: params", params->port, params->control,
                   params->formatChannels, params->formatIsFloat,
                   params->formatIsStd, params->freq, params->sampleLength);

  unsigned inChannels = 2;
  unsigned inSamples = params->sampleLength;
  sox_rate_t sampleRate = 48000; // probably there is no point to parse
                                 // frequency, because it's always 48000
  if (params->formatChannels == 2 && !params->formatIsFloat) {
    inChannels = 1;
    ORBIS_LOG_NOTICE(
        "AudioOut: format is ORBIS_AUDIO_OUT_PARAM_FORMAT_S16_MONO");
  } else if (params->formatChannels == 4 && !params->formatIsFloat) {
    inChannels = 2;
    ORBIS_LOG_NOTICE(
        "AudioOut: format is ORBIS_AUDIO_OUT_PARAM_FORMAT_S16_STEREO");
  } else if (params->formatChannels == 16 && !params->formatIsFloat &&
             !params->formatIsStd) {
    inChannels = 8;
    ORBIS_LOG_NOTICE(
        "AudioOut: format is ORBIS_AUDIO_OUT_PARAM_FORMAT_S16_8CH");
  } else if (params->formatChannels == 16 && !params->formatIsFloat &&
             params->formatIsStd) {
    inChannels = 8;
    ORBIS_LOG_NOTICE(
        "AudioOut: outputParam is ORBIS_AUDIO_OUT_PARAM_FORMAT_S16_8CH_STD");
  } else if (params->formatChannels == 4 && params->formatIsFloat) {
    inChannels = 1;
    ORBIS_LOG_NOTICE(
        "AudioOut: format is ORBIS_AUDIO_OUT_PARAM_FORMAT_FLOAT_MONO");
  } else if (params->formatChannels == 8 && params->formatIsFloat) {
    inChannels = 2;
    ORBIS_LOG_NOTICE(
        "AudioOut: format is ORBIS_AUDIO_OUT_PARAM_FORMAT_FLOAT_STEREO");
  } else if (params->formatChannels == 32 && params->formatIsFloat &&
             !params->formatIsStd) {
    inChannels = 8;
    ORBIS_LOG_NOTICE(
        "AudioOut: format is ORBIS_AUDIO_OUT_PARAM_FORMAT_FLOAT_8CH");
  } else if (params->formatChannels == 32 && params->formatIsFloat &&
             params->formatIsStd) {
    inChannels = 8;
    ORBIS_LOG_NOTICE("AudioOut: format is "
                     "ORBIS_AUDIO_OUT_PARAM_FORMAT_FLOAT_8CH_STD");
  } else {
    ORBIS_LOG_ERROR("AudioOut: unknown format type");
  }

  sox_signalinfo_t out_si = {
      .rate = sampleRate,
      .channels = inChannels,
      .precision = SOX_SAMPLE_PRECISION,
  };

  // need to be locked because libsox doesn't like simultaneous opening of the
  // output
  std::unique_lock lock(soxMtx);
  sox_format_t *output =
      sox_open_write("default", &out_si, NULL, "alsa", nullptr, nullptr);
  soxMtx.unlock();

  if (!output) {
    std::abort();
  }

  std::vector<sox_sample_t> samples(inSamples * inChannels);

  std::size_t clips = 0;
  SOX_SAMPLE_LOCALS;

  while (!exit.load(std::memory_order::relaxed)) {
    if (params->control == 0) {
      std::this_thread::sleep_for(std::chrono::microseconds(10));
      continue;
    }

    if (!params->formatIsFloat) {
      auto data = reinterpret_cast<const std::int16_t *>(audioBuffer);
      for (std::size_t i = 0; i < samples.size(); i++) {
        samples[i] = SOX_SIGNED_16BIT_TO_SAMPLE(data[i], clips);
      }
    } else {
      auto data = reinterpret_cast<const float *>(audioBuffer);
      for (std::size_t i = 0; i < samples.size(); i++) {
        samples[i] = SOX_FLOAT_32BIT_TO_SAMPLE(data[i], clips);
      }
    }

    if (sox_write(output, samples.data(), samples.size()) != samples.size()) {
      ORBIS_LOG_ERROR("AudioOut: sox_write failed");
    }

    // set zero to freeing audiooutput
    params->control = 0;

    // skip sceAudioOutMix%x event
    info.evf->set(bitPattern);
  }

  sox_close(output);

  ::munmap(audioBuffer, bufferStat.st_size);
  ::munmap(controlPtr, controlStat.st_size);

  ::close(controlFd);
  ::close(bufferFd);
}
