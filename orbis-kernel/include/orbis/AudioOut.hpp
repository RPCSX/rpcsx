#pragma once

#include "evf.hpp"
#include "utils/Logs.hpp"
#include <atomic>
#include <chrono>
#include <fcntl.h>
#include <mutex>
#include <sox.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <thread>
#include <vector>

namespace orbis {
struct AudioOutChannelInfo {
  std::int32_t port{};
  std::int32_t idControl{};
  std::int32_t channel{};
  Ref<EventFlag> evf;
};

struct AudioOut {
  std::mutex thrMtx;
  std::vector<std::thread> threads;
  AudioOutChannelInfo channelInfo;
  std::atomic<bool> exit{false};

  AudioOut() {
    if (sox_init() != SOX_SUCCESS) {
      ORBIS_LOG_FATAL("Failed to initialize sox");
      std::abort();
    }
  }

  ~AudioOut() {
    exit = true;
    for (auto &thread : threads) {
      thread.join();
    }
    sox_quit();
  }

  void start() {
    std::lock_guard lock(thrMtx);
    threads.push_back(std::thread(
        [this, channelInfo = channelInfo] { channelEntry(channelInfo); }));
  }

private:
  void channelEntry(AudioOutChannelInfo info) {
    char control_shm_name[32];
    char audio_shm_name[32];

    std::snprintf(control_shm_name, sizeof(control_shm_name), "/rpcsx-shm_%d_C",
                  info.idControl);
    std::snprintf(audio_shm_name, sizeof(audio_shm_name), "/rpcsx-shm_%d_%d_A",
                  info.channel, info.port);

    int controlFd =
        ::shm_open(control_shm_name, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    if (controlFd == -1) {
      perror("shm_open");
      std::abort();
    }

    struct stat controlStat;
    if (::fstat(controlFd, &controlStat)) {
      perror("shm_open");
      std::abort();
    }

    auto controlPtr = reinterpret_cast<std::uint8_t *>(
        ::mmap(NULL, controlStat.st_size, PROT_READ | PROT_WRITE, MAP_SHARED,
               controlFd, 0));
    if (controlPtr == MAP_FAILED) {
      perror("mmap");
      std::abort();
    }

    int bufferFd =
        ::shm_open(audio_shm_name, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    if (bufferFd == -1) {
      perror("open");
      std::abort();
    }

    struct stat bufferStat;
    if (::fstat(bufferFd, &bufferStat)) {
      perror("shm_open");
      std::abort();
    }

    auto audioBuffer = ::mmap(NULL, bufferStat.st_size, PROT_READ | PROT_WRITE,
                              MAP_SHARED, bufferFd, 0);
    auto bitPattern = 1u << info.port;

    int firstNonEmptyByteIndex = -1;

    for (std::size_t i = 24; i < controlStat.st_size; ++i) {
      if (controlPtr[i] != 0) {
        firstNonEmptyByteIndex = i - 8;
        break;
      }

      // FIXME: following line triggers error, investigate _C shm layout
      // std::this_thread::sleep_for(std::chrono::milliseconds(1));

      if (exit.load(std::memory_order::relaxed)) {
        break;
      }
    }

    if (firstNonEmptyByteIndex < 0) {
      ORBIS_LOG_ERROR("AudioOut: Failed to find first non zero byte index");
      std::abort();
    }

    int outParamFirstByte = controlPtr[firstNonEmptyByteIndex + 8];
    int isFloat = controlPtr[firstNonEmptyByteIndex + 44];

    // int outParamThirdByte = *((char *)controlPtr + firstNonEmptyByteIndex +
    // 44); // need to find the third index

    unsigned inChannels = 2;
    unsigned inSamples = 256;
    sox_rate_t sampleRate = 48000; // probably there is no point to parse
                                   // frequency, because it's always 48000
    if (outParamFirstByte == 2 && isFloat == 0) {
      inChannels = 1;
      ORBIS_LOG_NOTICE(
          "AudioOut: outputParam is ORBIS_AUDIO_OUT_PARAM_FORMAT_S16_MONO");
    } else if (outParamFirstByte == 4 && isFloat == 0) {
      inChannels = 2;
      ORBIS_LOG_NOTICE(
          "AudioOut: outputParam is ORBIS_AUDIO_OUT_PARAM_FORMAT_S16_STEREO");
    } else if (outParamFirstByte == 16 && isFloat == 0) {
      inChannels = 8;
      ORBIS_LOG_NOTICE(
          "AudioOut: outputParam is ORBIS_AUDIO_OUT_PARAM_FORMAT_S16_8CH");
    } else if (outParamFirstByte == 4 && isFloat == 1) {
      inChannels = 1;
      ORBIS_LOG_NOTICE(
          "AudioOut: outputParam is ORBIS_AUDIO_OUT_PARAM_FORMAT_FLOAT_MONO");
    } else if (outParamFirstByte == 8 && isFloat == 1) {
      inChannels = 2;
      ORBIS_LOG_NOTICE(
          "AudioOut: outputParam is ORBIS_AUDIO_OUT_PARAM_FORMAT_FLOAT_STEREO");
    } else if (outParamFirstByte == 32 && isFloat == 1) {
      inChannels = 8;
      ORBIS_LOG_NOTICE(
          "AudioOut: outputParam is ORBIS_AUDIO_OUT_PARAM_FORMAT_FLOAT_8CH");
    } else {
      ORBIS_LOG_ERROR("AudioOut: unknown output type");
    }

    // it's need third byte
    // if (outParamFirstByte == 16 && outParamSecondByte == 0 &&
    // outParamThirdByte
    // == 1) {
    //   printf("outputParam is ORBIS_AUDIO_OUT_PARAM_FORMAT_S16_8CH_STD");
    // }
    // if (outParamFirstByte == 32 && outParamSecondByte == 1 &&
    // outParamThirdByte
    // == 1) {
    //   printf("outputParam is ORBIS_AUDIO_OUT_PARAM_FORMAT_FLOAT_8CH_STD");
    // }

    // length byte will be inited after some time, so we wait for it
    int samplesLengthByte = 0;
    while (true) {
      samplesLengthByte = controlPtr[firstNonEmptyByteIndex + 97];
      if (samplesLengthByte > 0) {
        break;
      }
    }

    inSamples = samplesLengthByte * 256;

    sox_signalinfo_t out_si = {
        .rate = sampleRate,
        .channels = inChannels,
        .precision = SOX_SAMPLE_PRECISION,
    };

    sox_format_t *output =
        sox_open_write("default", &out_si, NULL, "alsa", NULL, NULL);
    if (!output) {
      std::abort();
    }

    std::vector<sox_sample_t> samples(inSamples * inChannels);

    std::size_t clips = 0;
    SOX_SAMPLE_LOCALS;

    while (!exit.load(std::memory_order::relaxed)) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));

      if (isFloat == 0) {
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

      // skip sceAudioOutMix%x event
      info.evf->set(bitPattern);

      // set zero to freeing audiooutput
      for (size_t i = 0; i < 8; ++i) {
        controlPtr[firstNonEmptyByteIndex + i] = 0;
      }
    }

    sox_close(output);

    ::munmap(audioBuffer, bufferStat.st_size);
    ::munmap(controlPtr, controlStat.st_size);

    ::close(controlFd);
    ::close(bufferFd);
  }
};
} // namespace orbis
