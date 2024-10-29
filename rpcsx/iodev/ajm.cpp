#include "ajm.hpp"
#include "io-device.hpp"
#include "libatrac9/libatrac9.h"
#include "orbis-config.hpp"
#include "orbis/KernelAllocator.hpp"
#include "orbis/file.hpp"
#include "orbis/thread/Thread.hpp"
#include "orbis/utils/Logs.hpp"
#include <cstdint>
#include <cstring>
#include <map>
#include <mutex>
#include <rx/atScopeExit.hpp>
#include <rx/hexdump.hpp>
extern "C" {
#include <libatrac9/decoder.h>
#include <libavcodec/avcodec.h>
#include <libavcodec/codec_internal.h>
#include <libavcodec/packet.h>
#include <libavformat/avformat.h>
#include <libavutil/mem.h>
#include <libavutil/opt.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
}

struct AjmFile : orbis::File {};

struct AjmDevice : IoDevice {
  // TODO: remove when moving input and temp buffers to instance
  orbis::shared_mutex mtx;
  // TODO: move to intsance
  orbis::kvector<std::byte> inputBuffer;
  orbis::uint32_t inputSize = 0;
  orbis::kvector<std::byte> tempBuffer;

  orbis::uint32_t batchId = 1; // temp

  orbis::uint32_t at9InstanceId = 0;
  orbis::uint32_t mp3InstanceId = 0;
  orbis::uint32_t aacInstanceId = 0;
  orbis::uint32_t unimplementedInstanceId = 0;
  orbis::kmap<orbis::int32_t, Instance> instanceMap;
  orbis::ErrorCode open(orbis::Ref<orbis::File> *file, const char *path,
                        std::uint32_t flags, std::uint32_t mode,
                        orbis::Thread *thread) override;
};

AVSampleFormat ajmToAvFormat(AJMFormat ajmFormat) {
  switch (ajmFormat) {
  case AJM_FORMAT_S16:
    return AV_SAMPLE_FMT_S16;
  case AJM_FORMAT_S32:
    return AV_SAMPLE_FMT_S32;
  case AJM_FORMAT_FLOAT:
    return AV_SAMPLE_FMT_FLTP;
  default:
    return AV_SAMPLE_FMT_NONE;
  }
}

void reset(Instance *instance) {
  instance->gapless.skipSamples = 0;
  instance->gapless.totalSamples = 0;
  instance->gapless.totalSkippedSamples = 0;
  instance->processedSamples = 0;
}

void resetAt9(Instance *instance) {
  if (instance->at9.configData) {
    Atrac9ReleaseHandle(instance->at9.handle);
    instance->at9.estimatedSizeUsed = 0;
    instance->at9.superFrameSize = 0;
    instance->at9.framesInSuperframe = 0;
    instance->at9.frameSamples = 0;
    instance->at9.sampleRate = 0;
    instance->at9.superFrameDataIdx = 0;
    instance->at9.inputChannels = 0;
    instance->at9.superFrameDataLeft = 0;
    instance->at9.handle = Atrac9GetHandle();
    int err = Atrac9InitDecoder(instance->at9.handle,
                                (uint8_t *)&instance->at9.configData);
    if (err < 0) {
      ORBIS_LOG_FATAL("AT9 Init Decoder error", err);
      std::abort();
    }
    Atrac9CodecInfo pCodecInfo;
    Atrac9GetCodecInfo(instance->at9.handle, &pCodecInfo);

    instance->at9.frameSamples = pCodecInfo.frameSamples;
    instance->at9.inputChannels = pCodecInfo.channels;
    instance->at9.framesInSuperframe = pCodecInfo.framesInSuperframe;
    instance->at9.superFrameDataIdx = 0;
    instance->at9.superFrameSize = pCodecInfo.superframeSize;
    instance->at9.superFrameDataLeft = pCodecInfo.superframeSize;
    instance->at9.sampleRate = pCodecInfo.samplingRate;
  }
}

static orbis::ErrorCode ajm_ioctl(orbis::File *file, std::uint64_t request,
                                  void *argp, orbis::Thread *thread) {

  auto device = static_cast<AjmDevice *>(file->device.get());
  std::lock_guard lock(device->mtx);
  // 0xc0288900 - finalize
  // 0xc0288903 - module register
  // 0xc0288904 - module unregister
  // 0xc0288905 - instance create
  // 0xc0288906 - instance destroy
  // 0xc028890a - instance extend
  // 0xc028890b - intasnce switch
  // 0xc0288907 - start batch buffer
  // 0xc0288908 - wait batch buffer
  // 0xc0288900 - unregister context
  if (request == 0xc0288906) {
    struct InstanceDestroyArgs {
      orbis::uint32_t result;
      orbis::uint32_t unk0;
      orbis::uint32_t instanceId;
    };
    auto args = reinterpret_cast<InstanceDestroyArgs *>(argp);
    auto it = device->instanceMap.find(args->instanceId);
    if (it != device->instanceMap.end()) {
      auto &instance = device->instanceMap[args->instanceId];
      if (instance.resampler) {
        swr_free(&instance.resampler);
      }
      if (instance.codecCtx) {
        avcodec_free_context(&instance.codecCtx);
      }
      device->instanceMap.erase(it);
    }
    args->result = 0;
  }
  if (request == 0xc0288903 || request == 0xc0288904 || request == 0xc0288900) {
    auto arg = reinterpret_cast<std::uint32_t *>(argp)[2];
    ORBIS_LOG_ERROR(__FUNCTION__, request, arg);
    *reinterpret_cast<std::uint64_t *>(argp) = 0;
    // return{};
  }
  if (request == 0xc0288905) {
    struct InstanceCreateArgs {
      orbis::uint32_t result;
      orbis::uint32_t unk0;
      orbis::uint64_t flags;
      orbis::uint32_t codec;
      orbis::uint32_t instanceId;
    };
    auto args = reinterpret_cast<InstanceCreateArgs *>(argp);
    AJMCodecs codecId = AJMCodecs(args->codec);
    auto codecOffset = codecId << 0xe;
    if (codecId >= 0 && codecId <= 2) {
      args->result = 0;
      if (codecId == AJM_CODEC_At9) {
        args->instanceId = codecOffset + device->at9InstanceId++;
      } else if (codecId == AJM_CODEC_MP3) {
        args->instanceId = codecOffset + device->mp3InstanceId++;
      } else if (codecId == AJM_CODEC_AAC) {
        args->instanceId = codecOffset + device->aacInstanceId++;
      }
      Instance instance;
      instance.codec = codecId;
      instance.maxChannels =
          AJMChannels(((args->flags & ~7) & (0xFF & ~0b11)) >> 3);
      instance.outputFormat = AJMFormat((args->flags & ~7) & 0b11);
      if (codecId == AJM_CODEC_At9) {
        instance.at9.handle = Atrac9GetHandle();
        if (instance.outputFormat == AJM_FORMAT_S16) {
          instance.at9.outputFormat = kAtrac9FormatS16;
        } else if (instance.outputFormat == AJM_FORMAT_S32) {
          instance.at9.outputFormat = kAtrac9FormatS32;
        } else if (instance.outputFormat == AJM_FORMAT_FLOAT) {
          instance.at9.outputFormat = kAtrac9FormatF32;
        } else {
          // TODO: throw error
        }
      }
      if (codecId == AJM_CODEC_AAC || codecId == AJM_CODEC_MP3) {
        const AVCodec *codec = avcodec_find_decoder(
            codecId == AJM_CODEC_AAC ? AV_CODEC_ID_AAC : AV_CODEC_ID_MP3);
        if (!codec) {
          ORBIS_LOG_FATAL("Codec not found", (orbis::uint32_t)codecId);
          std::abort();
        }
        AVCodecContext *codecCtx = avcodec_alloc_context3(codec);
        if (!codecCtx) {
          ORBIS_LOG_FATAL("Failed to allocate codec context");
          std::abort();
        }

        if (int err = avcodec_open2(codecCtx, codec, NULL) < 0) {
          ORBIS_LOG_FATAL("Could not open codec");
          std::abort();
        }

        instance.codecCtx = codecCtx;
      }
      device->instanceMap.insert({
          args->instanceId,
          instance,
      });
    } else {
      args->instanceId = codecOffset + device->unimplementedInstanceId++;
    }
    ORBIS_LOG_ERROR(__FUNCTION__, request, args->result, args->unk0,
                    args->flags, args->codec, args->instanceId);
  } else if (request == 0xc0288907) {
    struct StartBatchBufferArgs {
      orbis::uint32_t result;
      orbis::uint32_t unk0;
      std::byte *pBatch;
      orbis::uint32_t batchSize;
      orbis::uint32_t priority;
      orbis::uint64_t batchError;
      orbis::uint32_t batchId;
    };
    auto args = reinterpret_cast<StartBatchBufferArgs *>(argp);
    args->result = 0;
    args->batchId = device->batchId;
    // ORBIS_LOG_ERROR(__FUNCTION__, request, args->result, args->unk0,
    //                 args->pBatch, args->batchSize, args->priority,
    //                 args->batchError, args->batchId);
    device->batchId += 1;
    // thread->where();

    auto ptr = args->pBatch;
    auto endPtr = args->pBatch + args->batchSize;

    while (ptr < endPtr) {
      auto header = (InstructionHeader *)ptr;
      auto instanceId = (header->id >> 6) & 0xfffff;
      auto jobPtr = ptr + sizeof(InstructionHeader);
      auto endJobPtr = ptr + header->len;
      // TODO: handle unimplemented codecs, so auto create instance for now
      auto &instance = device->instanceMap[instanceId];
      RunJob runJob{};
      device->inputSize = 0;
      while (jobPtr < endJobPtr) {
        auto typed = (OpcodeHeader *)jobPtr;
        switch (typed->getOpcode()) {
        case Opcode::ReturnAddress: {
          // ReturnAddress *ra = (ReturnAddress *)jobPtr;
          // ORBIS_LOG_ERROR(__FUNCTION__, request, "return address",
          // ra->opcode,
          //                 ra->unk, ra->returnAddress);
          jobPtr += sizeof(ReturnAddress);
          break;
        }
        case Opcode::ControlBufferRa: {
          runJob.control = true;
          BatchJobControlBufferRa *ctrl = (BatchJobControlBufferRa *)jobPtr;
          AJMSidebandResult *result =
              reinterpret_cast<AJMSidebandResult *>(ctrl->pSidebandOutput);
          result->result = 0;
          result->codecResult = 0;
          ORBIS_LOG_ERROR(__FUNCTION__, request, "control buffer", ctrl->opcode,
                          ctrl->commandId, ctrl->flagsHi, ctrl->flagsLo,
                          ctrl->sidebandInputSize, ctrl->sidebandOutputSize);
          if (ctrl->getFlags() & CONTROL_RESET) {
            reset(&instance);
            if (instance.codec == AJM_CODEC_At9) {
              resetAt9(&instance);
            }
          }

          if (ctrl->getFlags() & CONTROL_INITIALIZE) {
            if (instance.codec == AJM_CODEC_At9) {
              struct InitalizeBuffer {
                orbis::uint32_t configData;
                orbis::int32_t unk0[2];
              };
              InitalizeBuffer *initializeBuffer =
                  (InitalizeBuffer *)ctrl->pSidebandInput;
              instance.at9.configData = initializeBuffer->configData;
              reset(&instance);
              resetAt9(&instance);

              orbis::uint32_t maxChannels =
                  instance.maxChannels == AJM_CHANNEL_DEFAULT
                      ? 2
                      : instance.maxChannels;
              orbis::uint32_t outputChannels =
                  instance.at9.inputChannels > maxChannels
                      ? maxChannels
                      : instance.at9.inputChannels;
              // TODO: check max channels
              ORBIS_LOG_TODO("CONTROL_INITIALIZE", instance.at9.inputChannels,
                             instance.at9.sampleRate, instance.at9.frameSamples,
                             instance.at9.superFrameSize, maxChannels,
                             outputChannels, initializeBuffer->configData,
                             (orbis::uint32_t)instance.outputFormat);
            } else if (instance.codec == AJM_CODEC_AAC) {
              struct InitalizeBuffer {
                orbis::uint32_t headerIndex;
                orbis::uint32_t sampleRateIndex;
              };
              InitalizeBuffer *initializeBuffer =
                  (InitalizeBuffer *)ctrl->pSidebandInput;
              instance.aac.headerType =
                  AACHeaderType(initializeBuffer->headerIndex);
              instance.aac.sampleRate =
                  AACFreq[initializeBuffer->sampleRateIndex];
            }
          }
          if (ctrl->getFlags() & SIDEBAND_GAPLESS_DECODE) {
            struct InitalizeBuffer {
              orbis::uint32_t totalSamples;
              orbis::uint16_t skipSamples;
              orbis::uint16_t totalSkippedSamples;
            };
            InitalizeBuffer *initializeBuffer =
                (InitalizeBuffer *)ctrl->pSidebandInput;
            if (initializeBuffer->totalSamples > 0) {
              instance.gapless.totalSamples = initializeBuffer->totalSamples;
            }
            if (initializeBuffer->skipSamples > 0) {
              instance.gapless.skipSamples = initializeBuffer->skipSamples;
            }
            ORBIS_LOG_TODO("SIDEBAND_GAPLESS_DECODE",
                           instance.gapless.skipSamples,
                           instance.gapless.totalSamples);
          }
          jobPtr += sizeof(BatchJobControlBufferRa);
          break;
        }
        case Opcode::RunBufferRa: {
          BatchJobInputBufferRa *job = (BatchJobInputBufferRa *)jobPtr;
          // ORBIS_LOG_ERROR(__FUNCTION__, request, "BatchJobInputBufferRa",
          //                 job->opcode, job->szInputSize, job->pInput);
          // We don't support split buffers for now, so ignore new buffers
          if (device->inputSize == 0) {
            std::memcpy(device->inputBuffer.data(), job->pInput,
                        job->szInputSize);
            device->inputSize += job->szInputSize;
          }
          // rx::hexdump({(std::byte*) job->pInput, job->szInputSize});
          jobPtr += sizeof(BatchJobInputBufferRa);
          break;
        }
        case Opcode::Flags: {
          BatchJobFlagsRa *job = (BatchJobFlagsRa *)jobPtr;
          // ORBIS_LOG_ERROR(__FUNCTION__, request, "BatchJobFlagsRa",
          //                 job->flagsHi, job->flagsLo);
          runJob.flags = ((orbis::uint64_t)job->flagsHi << 0x1a) | job->flagsLo;
          jobPtr += sizeof(BatchJobFlagsRa);
          break;
        }
        case Opcode::JobBufferOutputRa: {
          BatchJobOutputBufferRa *job = (BatchJobOutputBufferRa *)jobPtr;
          // ORBIS_LOG_ERROR(__FUNCTION__, request, "BatchJobOutputBufferRa",
          //                 job->opcode, job->outputSize, job->pOutput);
          runJob.pOutput = job->pOutput;
          runJob.outputSize = job->outputSize;
          jobPtr += sizeof(BatchJobOutputBufferRa);
          break;
        }
        case Opcode::JobBufferSidebandRa: {
          BatchJobSidebandBufferRa *job = (BatchJobSidebandBufferRa *)jobPtr;
          // ORBIS_LOG_ERROR(__FUNCTION__, request, "BatchJobSidebandBufferRa",
          //                 job->opcode, job->sidebandSize, job->pSideband);
          runJob.pSideband = job->pSideband;
          runJob.sidebandSize = job->sidebandSize;
          jobPtr += sizeof(BatchJobSidebandBufferRa);
          break;
        }
        default:
          jobPtr = endJobPtr;
        }
      }
      ptr = jobPtr;
      if (!runJob.control && instanceId >= 0xC000) {
        AJMSidebandResult *result =
            reinterpret_cast<AJMSidebandResult *>(runJob.pSideband);
        result->result = 0;
        result->codecResult = 0;
        if (runJob.flags & SIDEBAND_STREAM) {
          AJMSidebandStream *stream =
              reinterpret_cast<AJMSidebandStream *>(runJob.pSideband + 8);
          stream->inputSize = device->inputSize;
          stream->outputSize = runJob.outputSize;
        }
      } else if (!runJob.control) {
        // orbis::uint32_t maxChannels =
        //     instance.maxChannels == AJM_CHANNEL_DEFAULT ? 2
        //                                                 :
        //                                                 instance.maxChannels;
        AJMSidebandResult *result =
            reinterpret_cast<AJMSidebandResult *>(runJob.pSideband);
        result->result = 0;
        result->codecResult = 0;

        orbis::uint32_t inputReaded = 0;
        orbis::uint32_t outputWritten = 0;
        orbis::uint32_t framesProcessed = 0;
        orbis::uint32_t samplesCount = 0;
        if (device->inputSize != 0 && runJob.outputSize != 0) {
          do {
            if (instance.codec == AJM_CODEC_At9 &&
                instance.at9.frameSamples == 0) {
              break;
            }
            if (inputReaded >= device->inputSize ||
                outputWritten >= runJob.outputSize) {
              break;
            }

            AVFrame *frame = av_frame_alloc();
            rx::atScopeExit _free_frame([&] { av_frame_free(&frame); });

            orbis::uint32_t readed = 0;
            orbis::uint32_t outputBufferSize = 0;
            if (instance.codec == AJM_CODEC_At9) {
              orbis::int32_t bytesUsed = 0;
              outputBufferSize = av_samples_get_buffer_size(
                  nullptr, instance.at9.inputChannels,
                  instance.at9.frameSamples,
                  ajmToAvFormat(instance.outputFormat), 0);
              int err = Atrac9Decode(instance.at9.handle,
                                     &device->inputBuffer[inputReaded],
                                     device->tempBuffer.data(),
                                     instance.at9.outputFormat, &bytesUsed);
              if (err != ERR_SUCCESS) {
                rx::hexdump({(std::byte *)&device->inputBuffer[inputReaded],
                             device->inputSize});
                ORBIS_LOG_FATAL("Could not decode frame", err,
                                instance.at9.estimatedSizeUsed,
                                instance.at9.superFrameSize,
                                instance.at9.frameSamples, instance.at9.handle,
                                inputReaded, outputWritten);
                std::abort();
              }
              instance.at9.estimatedSizeUsed =
                  static_cast<orbis::uint32_t>(bytesUsed);
              instance.at9.superFrameDataLeft -= bytesUsed;
              instance.at9.superFrameDataIdx++;
              if (instance.at9.superFrameDataIdx ==
                  instance.at9.framesInSuperframe) {
                instance.at9.estimatedSizeUsed +=
                    instance.at9.superFrameDataLeft;
                instance.at9.superFrameDataIdx = 0;
                instance.at9.superFrameDataLeft = instance.at9.superFrameSize;
              }
              samplesCount = instance.at9.frameSamples;
              readed = instance.at9.estimatedSizeUsed;
              instance.lastDecode.channels =
                  AJMChannels(instance.at9.inputChannels);
              instance.lastDecode.sampleRate = instance.at9.sampleRate;
              // ORBIS_LOG_TODO("at9 decode", instance.at9.estimatedSizeUsed,
              //                instance.at9.superFrameDataLeft,
              //                instance.at9.superFrameDataIdx,
              //                instance.at9.framesInSuperframe);
            } else if (instance.codec == AJM_CODEC_MP3) {
              auto frameSize = get_mp3_data_size(
                  (orbis::uint8_t *)(&device->inputBuffer[inputReaded]));
              if (frameSize == 0) {
                frameSize = device->inputSize;
              } else {
                frameSize = std::min(frameSize, device->inputSize);
              }
              AVPacket *pkt = av_packet_alloc();
              rx::atScopeExit _free_pkt([&] { av_packet_free(&pkt); });
              pkt->data = (orbis::uint8_t *)(&device->inputBuffer[inputReaded]);
              pkt->size = frameSize;
              int ret = avcodec_send_packet(instance.codecCtx, pkt);
              if (ret < 0) {
                ORBIS_LOG_FATAL("Error sending packet for decoding", ret);
                std::abort();
              }
              ret = avcodec_receive_frame(instance.codecCtx, frame);
              if (ret < 0) {
                ORBIS_LOG_FATAL("Error during decoding");
                std::abort();
              }
              outputBufferSize = av_samples_get_buffer_size(
                  nullptr, frame->ch_layout.nb_channels, frame->nb_samples,
                  ajmToAvFormat(instance.outputFormat), 0);
              samplesCount = frame->nb_samples;
              readed = frameSize;
              instance.lastDecode.channels =
                  AJMChannels(frame->ch_layout.nb_channels);
              instance.lastDecode.sampleRate = frame->sample_rate;
            } else if (instance.codec == AJM_CODEC_AAC) {
              AVPacket *pkt = av_packet_alloc();
              rx::atScopeExit _free_pkt([&] { av_packet_free(&pkt); });
              pkt->data = (orbis::uint8_t *)(&device->inputBuffer[inputReaded]);
              pkt->size = device->inputSize;

              // HACK: to avoid writing a bunch of useless calls
              // we simply call this method directly (but it can be very
              // unstable)
              int gotFrame;
              int len =
                  ffcodec(instance.codecCtx->codec)
                      ->cb.decode(instance.codecCtx, frame, &gotFrame, pkt);
              if (len < 0) {
                ORBIS_LOG_FATAL("Error during decoding");
                std::abort();
              }
              outputBufferSize = av_samples_get_buffer_size(
                  nullptr, frame->ch_layout.nb_channels, frame->nb_samples,
                  ajmToAvFormat(instance.outputFormat), 0);
              samplesCount = frame->nb_samples;
              readed = len;
              instance.lastDecode.channels =
                  AJMChannels(frame->ch_layout.nb_channels);
              instance.lastDecode.sampleRate = frame->sample_rate;
            }
            framesProcessed += 1;
            inputReaded += readed;

            if (instance.gapless.skipSamples > 0 ||
                instance.gapless.totalSamples > 0) {
              if (instance.gapless.skipSamples >
                      instance.gapless.totalSkippedSamples ||
                  instance.processedSamples > instance.gapless.totalSamples) {
                instance.gapless.totalSkippedSamples += samplesCount;
                continue;
              }
            }
            // at least three codecs outputs in float
            // and mp3 support sample rate resample (TODO), so made resampling
            // with swr
            if (instance.codec != AJM_CODEC_At9) {
              auto resampler = swr_alloc();
              AVChannelLayout chLayout;
              av_channel_layout_default(&chLayout,
                                        frame->ch_layout.nb_channels);
              av_opt_set_chlayout(resampler, "in_chlayout", &chLayout, 0);
              av_opt_set_chlayout(resampler, "out_chlayout", &chLayout, 0);
              av_opt_set_int(resampler, "in_sample_rate", frame->sample_rate,
                             0);
              av_opt_set_int(resampler, "out_sample_rate", frame->sample_rate,
                             0);
              av_opt_set_sample_fmt(resampler, "in_sample_fmt",
                                    ajmToAvFormat(AJM_FORMAT_FLOAT), 0);
              av_opt_set_sample_fmt(resampler, "out_sample_fmt",
                                    ajmToAvFormat(instance.outputFormat), 0);
              if (swr_init(resampler) < 0) {
                ORBIS_LOG_FATAL("Failed to initialize the resampling context");
                std::abort();
              }
              orbis::uint8_t *outputBuffer =
                  reinterpret_cast<orbis::uint8_t *>(device->tempBuffer.data());
              int nb_samples =
                  swr_convert(resampler, &outputBuffer, frame->nb_samples,
                              (const orbis::uint8_t **)frame->extended_data,
                              frame->nb_samples);
              if (nb_samples < 0) {
                ORBIS_LOG_FATAL("Error while converting");
                std::abort();
              }
            }
            memcpy(runJob.pOutput + outputWritten, device->tempBuffer.data(),
                   outputBufferSize);
            outputWritten += outputBufferSize;
            instance.processedSamples += samplesCount;
          } while ((runJob.flags & RUN_MULTIPLE_FRAMES) != 0);
        }

        orbis::int64_t currentSize = sizeof(AJMSidebandResult);

        if (runJob.flags & SIDEBAND_STREAM) {
          // ORBIS_LOG_TODO("SIDEBAND_STREAM", currentSize, inputReaded,
          //                outputWritten, instance.processedSamples);
          AJMSidebandStream *stream = reinterpret_cast<AJMSidebandStream *>(
              runJob.pSideband + currentSize);
          stream->inputSize = inputReaded;
          stream->outputSize = outputWritten;
          stream->decodedSamples = instance.processedSamples;
          currentSize += sizeof(AJMSidebandStream);
        }

        if (runJob.flags & SIDEBAND_FORMAT) {
          // ORBIS_LOG_TODO("SIDEBAND_FORMAT", currentSize);
          AJMSidebandFormat *format = reinterpret_cast<AJMSidebandFormat *>(
              runJob.pSideband + currentSize);
          format->channels = AJMChannels(instance.lastDecode.channels);
          format->sampleRate = instance.lastDecode.sampleRate;
          format->sampleFormat = instance.outputFormat;
          // TODO: channel mask and bitrate
          currentSize += sizeof(AJMSidebandFormat);
        }

        if (runJob.flags & SIDEBAND_GAPLESS_DECODE) {
          // ORBIS_LOG_TODO("SIDEBAND_GAPLESS_DECODE", currentSize);
          AJMSidebandGaplessDecode *gapless =
              reinterpret_cast<AJMSidebandGaplessDecode *>(runJob.pSideband +
                                                           currentSize);
          gapless->skipSamples = instance.gapless.skipSamples;
          gapless->totalSamples = instance.gapless.totalSamples;
          gapless->totalSkippedSamples = instance.gapless.totalSkippedSamples;
          currentSize += sizeof(AJMSidebandGaplessDecode);
        }

        if (runJob.flags & RUN_GET_CODEC_INFO) {
          // ORBIS_LOG_TODO("RUN_GET_CODEC_INFO");
          if (instance.codec == AJM_CODEC_At9) {
            AJMAt9CodecInfoSideband *info =
                reinterpret_cast<AJMAt9CodecInfoSideband *>(runJob.pSideband +
                                                            currentSize);
            info->superFrameSize = instance.at9.superFrameSize;
            info->framesInSuperFrame = instance.at9.framesInSuperframe;
            info->frameSamples = instance.at9.frameSamples;
            currentSize += sizeof(AJMAt9CodecInfoSideband);
          } else if (instance.codec == AJM_CODEC_MP3) {
            // TODO
            AJMMP3CodecInfoSideband *info =
                reinterpret_cast<AJMMP3CodecInfoSideband *>(runJob.pSideband +
                                                            currentSize);
            currentSize += sizeof(AJMMP3CodecInfoSideband);
          } else if (instance.codec == AJM_CODEC_AAC) {
            // TODO
            AJMAACCodecInfoSideband *info =
                reinterpret_cast<AJMAACCodecInfoSideband *>(runJob.pSideband +
                                                            currentSize);
            currentSize += sizeof(AJMAACCodecInfoSideband);
          }
        }

        if (runJob.flags & RUN_MULTIPLE_FRAMES) {
          // ORBIS_LOG_TODO("RUN_MULTIPLE_FRAMES", framesProcessed);
          AJMSidebandMultipleFrames *multipleFrames =
              reinterpret_cast<AJMSidebandMultipleFrames *>(runJob.pSideband +
                                                            currentSize);
          multipleFrames->framesProcessed = framesProcessed;
          currentSize += sizeof(AJMSidebandMultipleFrames);
        }
      }
    }
  } else if (request == 0xc0288908) {
    struct Args {
      orbis::uint32_t unk0;
      orbis::uint32_t unk1;
      orbis::uint32_t batchId;
      orbis::uint32_t timeout;
      orbis::uint64_t batchError;
    };
    auto args = reinterpret_cast<Args *>(argp);
    args->unk0 = 0;
    // ORBIS_LOG_ERROR(__FUNCTION__, request, args->unk0, args->unk1,
    //                 args->batchId, args->timeout, args->batchError);
    // thread->where();
  } else {
    ORBIS_LOG_FATAL("Unhandled AJM ioctl", request);
    thread->where();
  }
  return {};
}

static const orbis::FileOps fileOps = {
    .ioctl = ajm_ioctl,
};

orbis::ErrorCode AjmDevice::open(orbis::Ref<orbis::File> *file,
                                 const char *path, std::uint32_t flags,
                                 std::uint32_t mode, orbis::Thread *thread) {
  auto newFile = orbis::knew<AjmFile>();
  newFile->ops = &fileOps;
  newFile->device = this;

  inputBuffer.reserve(32 * 1024);
  tempBuffer.reserve(32 * 1024);

  *file = newFile;
  return {};
}

IoDevice *createAjmCharacterDevice() { return orbis::knew<AjmDevice>(); }
