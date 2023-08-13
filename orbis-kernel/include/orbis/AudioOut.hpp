#pragma once

#include <condition_variable>
#include <forward_list>
#include <functional>
#include <map>
#include <mutex>
#include <sys/ucontext.h>
#include <thread>
#include <ucontext.h>
#include <utility>
#include <cstdio>
#include "sys/sysproto.hpp"
#include <pthread.h>
#include <sox.h>

struct args {
    int32_t audioPort;
    int32_t idControl;
    int32_t idAudio;
    orbis::Thread *thread;
    int32_t evfId;
};

int msleep(long msec)
{
    struct timespec ts;
    int res;

    if (msec < 0)
    {
        errno = EINVAL;
        return -1;
    }

    ts.tv_sec = msec / 1000;
    ts.tv_nsec = (msec % 1000) * 1000000;

    do {
        res = nanosleep(&ts, &ts);
    } while (res && errno == EINTR);

    return res;
}

void DumpHex(const void* data, size_t size) {
	char ascii[17];
	size_t i, j;
	ascii[16] = '\0';
	for (i = 0; i < size; ++i) {
		printf("%02X ", ((unsigned char*)data)[i]);
		if (((unsigned char*)data)[i] >= ' ' && ((unsigned char*)data)[i] <= '~') {
			ascii[i % 16] = ((unsigned char*)data)[i];
		} else {
			ascii[i % 16] = '.';
		}
		if ((i+1) % 8 == 0 || i+1 == size) {
			printf(" ");
			if ((i+1) % 16 == 0) {
				printf("|  %s \n", ascii);
			} else if (i+1 == size) {
				ascii[(i+1) % 16] = '\0';
				if ((i+1) % 16 <= 8) {
					printf(" ");
				}
				for (j = (i+1) % 16; j < 16; ++j) {
					printf("   ");
				}
				printf("|  %s \n", ascii);
			}
		}
	}
}

void * loop(void *vargp)
{
    size_t control_shm_size = 0x10000;
    size_t audio_shm_size = 65536;

    char control_shm_name[32];
    char audio_shm_name[32];

    sprintf(control_shm_name, "/rpcsx-shm_%d_C", ((struct args*)vargp)->idControl);
    sprintf(audio_shm_name, "/rpcsx-shm_%d_%d_A", ((struct args*)vargp)->idAudio, ((struct args*)vargp)->audioPort);

    int controlFd = shm_open(control_shm_name, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    if (controlFd == -1) {
        perror("shm_open");
        exit(EXIT_FAILURE);
    }
    void *controlPtr = mmap(NULL, control_shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, controlFd, 0);
    if (controlPtr == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }

    int audioFd = shm_open(audio_shm_name, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    if (audioFd == -1) {
      perror("open");
      exit(EXIT_FAILURE);
    }
	  void *audioPtr = mmap(NULL, audio_shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, audioFd, 0);

    int64_t controlPtrWithOffset = (int64_t)(controlPtr + 8);

    int32_t bitPattern = 1 << ((struct args*)vargp)->audioPort;

    int firstNonEmptyByteIndex;

    for (size_t i = 24; i < control_shm_size; ++i) {
        if (*((char *)controlPtr + i) > 0) {
          firstNonEmptyByteIndex = i - 8;
          break;
        }
    }

    int outParamFirstByte = *((char *)controlPtr + firstNonEmptyByteIndex + 8);
    int isFloatByte = *((char *)controlPtr + firstNonEmptyByteIndex + 44);
    // int outParamThirdByte = *((char *)controlPtr + firstNonEmptyByteIndex + 44); // need to find the third index
    int in_channels = 2, in_samples = 256, sample_rate = 48000; // probably there is no point to parse frequency, because it's always 48000
    if (outParamFirstByte == 2 && isFloatByte == 0) {
      in_channels = 1;
      printf("outputParam is ORBIS_AUDIO_OUT_PARAM_FORMAT_S16_MONO\n");
    }
    if (outParamFirstByte == 4 && isFloatByte == 0) {
      in_channels = 2;
      printf("outputParam is ORBIS_AUDIO_OUT_PARAM_FORMAT_S16_STEREO\n");
    }
    if (outParamFirstByte == 16 && isFloatByte == 0) {
      in_channels = 8;
      printf("outputParam is ORBIS_AUDIO_OUT_PARAM_FORMAT_S16_8CH\n");
    }
    if (outParamFirstByte == 4 && isFloatByte == 1) {
      in_channels = 1;
      printf("outputParam is ORBIS_AUDIO_OUT_PARAM_FORMAT_FLOAT_MONO\n");
    }
    if (outParamFirstByte == 8 && isFloatByte == 1) {
      in_channels = 2;
      printf("outputParam is ORBIS_AUDIO_OUT_PARAM_FORMAT_FLOAT_STEREO\n");
    }
    if (outParamFirstByte == 32 && isFloatByte == 1) {
      in_channels = 8;
      printf("outputParam is ORBIS_AUDIO_OUT_PARAM_FORMAT_FLOAT_8CH\n");
    }
    // // it's need third byte
    // if (outParamFirstByte == 16 && outParamSecondByte == 0 && outParamThirdByte == 1) {
    //   printf("outputParam is ORBIS_AUDIO_OUT_PARAM_FORMAT_S16_8CH_STD");
    // }
    // if (outParamFirstByte == 32 && outParamSecondByte == 1 && outParamThirdByte == 1) {
    //   printf("outputParam is ORBIS_AUDIO_OUT_PARAM_FORMAT_FLOAT_8CH_STD");
    // }

    // length byte will be inited after some time, so we wait for it
    int samplesLengthByte;
    while(true) {
      samplesLengthByte = *((char *)controlPtr + firstNonEmptyByteIndex + 97);
      if (samplesLengthByte > 0) {
        break;
      }
    }

    in_samples = samplesLengthByte * 256;

    if (sox_init() != SOX_SUCCESS) {
			exit(1);
    }

    sox_signalinfo_t out_si = {};
    out_si.rate = sample_rate;
    out_si.channels = in_channels;
    out_si.precision = SOX_SAMPLE_PRECISION;

    sox_format_t* output
          = sox_open_write("default", &out_si, NULL, "alsa", NULL, NULL);
    if (!output) {
        exit(1);
    }

    sox_sample_t samples[in_samples * in_channels];

    size_t clips = 0; SOX_SAMPLE_LOCALS;
    size_t n_samples;
    int size;
    if (isFloatByte == 0) {
      size = in_samples * in_channels * sizeof(int16_t);
      n_samples = size / sizeof(int16_t);
    } else if (isFloatByte == 1) {
      size = in_samples * in_channels * sizeof(float);
      n_samples = size / sizeof(float);
    }
    while(true) {
      // skip sceAudioOutMix%x event
      sys_evf_set(((struct args*)vargp)->thread, ((struct args*)vargp)->evfId, bitPattern);
      // set zero to freeing audiooutput
      for (size_t i = 0; i < 8; ++i) {
        *((char *)controlPtr + firstNonEmptyByteIndex + i) = 0x00;
      }

      // DumpHex(audioPtr, 1000);
      // sleep 1ms
      msleep(1);
      if (isFloatByte == 0) {
        int16_t data[size];
		    memcpy(data, audioPtr, size);
        for (size_t n = 0; n < n_samples; n++) {
          samples[n] = SOX_SIGNED_16BIT_TO_SAMPLE(data[n], clips);
        }
        // free(data);
      }
      if (isFloatByte == 1) {
        float data[size];
		    memcpy(data, audioPtr, size);
        for (size_t n = 0; n < n_samples; n++) {
          samples[n] = SOX_FLOAT_32BIT_TO_SAMPLE(data[n], clips);
        }
        // free(data);
      }

      if (sox_write(output, samples, n_samples) != n_samples) {
          exit(1);
      }
    }
    pthread_exit(NULL);
}

namespace orbis {
class AudioOut {
public:
  int32_t audioPort;
  int32_t idControl;
  int32_t idAudio;
  int32_t evfId;
  AudioOut() {
  }

  ~AudioOut() {
  }

  static AudioOut& getInstance() {
      static AudioOut  instance;
      return instance;
  } 

  void setPortId(int32_t port) {
    this->audioPort = port;
  }

  void setControlId(int32_t id) {
    this->idControl = id;
  }

  void setAudioId(int32_t id) {
    this->idAudio = id;
  }

  void setEvfId(int32_t evfId) {
    this->evfId = evfId;
  }

  void start(orbis::Thread *thread) {
    Ref<File> file;
    // probably need to close
    auto result = thread->tproc->ops->open(thread, "/dev/audioHack", 0, 0, &file);
    if (result.value() == 0) {
      struct args *threadArgs = (struct args *)malloc(sizeof(struct args));
      threadArgs->audioPort = this->audioPort;
      threadArgs->idControl = this->idControl;
      threadArgs->idAudio = this->idAudio;
      threadArgs->thread = thread;
      threadArgs->evfId = this->evfId;

      pthread_t thread_id;
      pthread_create(&thread_id, NULL, loop, (void *)threadArgs);
    }
  }
  private: 
    AudioOut( const AudioOut&);
    AudioOut& operator=( AudioOut& );
};
} // namespace orbis
