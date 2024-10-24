#include "ajm.hpp"
#include "io-device.hpp"
#include "libatrac9/libatrac9.h"
#include "orbis-config.hpp"
#include "orbis/KernelAllocator.hpp"
#include "orbis/file.hpp"
#include "orbis/thread/Thread.hpp"
#include "orbis/utils/Logs.hpp"
#include <cstdint>
#include <map>
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

namespace ajm {
orbis::uint32_t batchId = 1;

orbis::uint32_t at9InstanceId = 0;
orbis::uint32_t mp3InstanceId = 0;
orbis::uint32_t aacInstanceId = 0;
orbis::uint32_t unimplementedInstanceId = 0;
orbis::kmap<orbis::int32_t, Instance> instanceMap;
} // namespace ajm

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

static orbis::ErrorCode ajm_ioctl(orbis::File *file, std::uint64_t request,
                                  void *argp, orbis::Thread *thread) {

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
    auto it = ajm::instanceMap.find(args->instanceId);
    if (it != ajm::instanceMap.end()) {
      auto &instance = ajm::instanceMap[args->instanceId];
      if (instance.resampler) {
        swr_free(&instance.resampler);
        avcodec_free_context(&instance.codecCtx);
      }
      ajm::instanceMap.erase(args->instanceId);
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
        args->instanceId = codecOffset + ajm::at9InstanceId++;
      } else if (codecId == AJM_CODEC_MP3) {
        args->instanceId = codecOffset + ajm::mp3InstanceId++;
      } else if (codecId == AJM_CODEC_AAC) {
        args->instanceId = codecOffset + ajm::aacInstanceId++;
      }
      Instance instance;
      instance.codec = codecId;
      instance.maxChannels =
          AJMChannels(((args->flags & ~7) & (0xFF & ~0b11)) >> 3);
      instance.outputFormat = AJMFormat((args->flags & ~7) & 0b11);
      if (codecId == AJM_CODEC_At9) {
        instance.at9.handle = Atrac9GetHandle();
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
      ajm::instanceMap.insert({
          args->instanceId,
          instance,
      });
    } else {
      args->instanceId = codecOffset + ajm::unimplementedInstanceId++;
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
    args->batchId = ajm::batchId;
    ORBIS_LOG_ERROR(__FUNCTION__, request, args->result, args->unk0,
                    args->pBatch, args->batchSize, args->priority,
                    args->batchError, args->batchId);
    ajm::batchId += 1;
    thread->where();

    auto ptr = args->pBatch;
    auto endPtr = args->pBatch + args->batchSize;

    while (ptr < endPtr) {
      auto header = (InstructionHeader *)ptr;
      auto instanceId = (header->id >> 6) & 0xfffff;
      auto jobPtr = ptr + sizeof(InstructionHeader);
      auto endJobPtr = ptr + header->len;
      // TODO: handle unimplemented codecs, so auto create instance for now
      auto &instance = ajm::instanceMap[instanceId];
      ORBIS_LOG_TODO("instance info", instanceId);
      RunJob runJob{};
      while (jobPtr < endJobPtr) {
        auto typed = (OpcodeHeader *)jobPtr;
        switch (typed->getOpcode()) {
        case Opcode::ReturnAddress: {
          ReturnAddress *ra = (ReturnAddress *)jobPtr;
          ORBIS_LOG_ERROR(__FUNCTION__, request, "return address", ra->opcode,
                          ra->unk, ra->returnAddress);
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
            // instance.outputChannels = AJMChannels(0);
            // instance.outputFormat = AJMFormat(0);
            instance.gaplessSkipSamples = 0;
            instance.gaplessTotalSamples = 0;
            instance.gaplessTotalSkippedSamples = 0;
            instance.processedSamples = 0;
            ORBIS_LOG_TODO("CONTROL_RESET");
          }

          if (ctrl->getFlags() & CONTROL_INITIALIZE) {
            if (instance.codec == AJM_CODEC_At9) {
              struct InitalizeBuffer {
                orbis::uint32_t configData;
                orbis::int32_t unk0[2];
              };
              InitalizeBuffer *initializeBuffer =
                  (InitalizeBuffer *)ctrl->pSidebandInput;
              Atrac9ReleaseHandle(instance.at9.handle);
              instance.at9.estimatedSizeUsed = 0;
              instance.at9.superFrameSize = 0;
              instance.at9.framesInSuperframe = 0;
              instance.at9.frameSamples = 0;
              instance.at9.sampleRate = 0;
              instance.at9.superFrameDataIdx = 0;
              instance.at9.inputChannels = 0;
              instance.at9.superFrameDataLeft = 0;
              instance.at9.handle = Atrac9GetHandle();
              int err = Atrac9InitDecoder(
                  instance.at9.handle,
                  reinterpret_cast<uint8_t *>(&initializeBuffer->configData));
              if (err < 0) {
                ORBIS_LOG_FATAL("AT9 Init Decoder error", err);
                rx::hexdump({(std::byte *)ctrl->pSidebandInput,
                             ctrl->sidebandInputSize});
                std::abort();
              }
              Atrac9CodecInfo pCodecInfo;
              Atrac9GetCodecInfo(instance.at9.handle, &pCodecInfo);

              instance.at9.frameSamples = pCodecInfo.frameSamples;
              instance.at9.inputChannels = pCodecInfo.channels;
              instance.at9.framesInSuperframe = pCodecInfo.framesInSuperframe;
              instance.at9.superFrameDataIdx = 0;
              instance.at9.superFrameSize = pCodecInfo.superframeSize;
              instance.at9.superFrameDataLeft = pCodecInfo.superframeSize;
              instance.at9.sampleRate = pCodecInfo.samplingRate;

              orbis::uint32_t maxChannels =
                  instance.maxChannels == AJM_CHANNEL_DEFAULT
                      ? 2
                      : instance.maxChannels;
              orbis::uint32_t outputChannels =
                  instance.at9.inputChannels > maxChannels
                      ? maxChannels
                      : instance.at9.inputChannels;
              if (instance.resampler) {
                swr_free(&instance.resampler);
                instance.resampler = NULL;
              }
              if (instance.at9.inputChannels != outputChannels ||
                  instance.outputFormat != AJM_FORMAT_S16) {
                instance.resampler = swr_alloc();
                if (!instance.resampler) {
                  ORBIS_LOG_FATAL("Could not allocate resampler context");
                  std::abort();
                }

                AVChannelLayout inputChLayout;
                av_channel_layout_default(&inputChLayout,
                                          instance.at9.inputChannels);

                AVChannelLayout outputChLayout;
                av_channel_layout_default(&outputChLayout, outputChannels);

                av_opt_set_chlayout(instance.resampler, "in_chlayout",
                                    &inputChLayout, 0);
                av_opt_set_chlayout(instance.resampler, "out_chlayout",
                                    &outputChLayout, 0);
                av_opt_set_int(instance.resampler, "in_sample_rate",
                               pCodecInfo.samplingRate, 0);
                av_opt_set_int(instance.resampler, "out_sample_rate",
                               pCodecInfo.samplingRate, 0);
                av_opt_set_sample_fmt(instance.resampler, "in_sample_fmt",
                                      ajmToAvFormat(AJM_FORMAT_S16), 0);
                av_opt_set_sample_fmt(instance.resampler, "out_sample_fmt",
                                      ajmToAvFormat(instance.outputFormat), 0);
                if (swr_init(instance.resampler) < 0) {
                  ORBIS_LOG_FATAL(
                      "Failed to initialize the resampling context");
                  std::abort();
                }
              }
              ORBIS_LOG_TODO("CONTROL_INITIALIZE", pCodecInfo.channels,
                             pCodecInfo.samplingRate, pCodecInfo.frameSamples,
                             pCodecInfo.superframeSize, maxChannels,
                             outputChannels, initializeBuffer->configData,
                             &initializeBuffer->configData);
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
              instance.gaplessTotalSamples = initializeBuffer->totalSamples;
            }
            if (initializeBuffer->skipSamples > 0) {
              instance.gaplessSkipSamples = initializeBuffer->skipSamples;
            }
            ORBIS_LOG_TODO("SIDEBAND_GAPLESS_DECODE",
                           instance.gaplessSkipSamples,
                           instance.gaplessTotalSamples);
          }
          jobPtr += sizeof(BatchJobControlBufferRa);
          break;
        }
        case Opcode::RunBufferRa: {
          BatchJobInputBufferRa *job = (BatchJobInputBufferRa *)jobPtr;
          ORBIS_LOG_ERROR(__FUNCTION__, request, "BatchJobInputBufferRa",
                          job->opcode, job->szInputSize, job->pInput);
          runJob.pInput = job->pInput;
          runJob.inputSize = job->szInputSize;
          // rx::hexdump({(std::byte*) job->pInput, job->szInputSize});
          jobPtr += sizeof(BatchJobInputBufferRa);
          break;
        }
        case Opcode::Flags: {
          BatchJobFlagsRa *job = (BatchJobFlagsRa *)jobPtr;
          ORBIS_LOG_ERROR(__FUNCTION__, request, "BatchJobFlagsRa",
                          job->flagsHi, job->flagsLo);
          runJob.flags = ((uint64_t)job->flagsHi << 0x1a) | job->flagsLo;
          jobPtr += sizeof(BatchJobFlagsRa);
          break;
        }
        case Opcode::JobBufferOutputRa: {
          BatchJobOutputBufferRa *job = (BatchJobOutputBufferRa *)jobPtr;
          ORBIS_LOG_ERROR(__FUNCTION__, request, "BatchJobOutputBufferRa",
                          job->opcode, job->outputSize, job->pOutput);
          runJob.pOutput = job->pOutput;
          runJob.outputSize = job->outputSize;
          jobPtr += sizeof(BatchJobOutputBufferRa);
          break;
        }
        case Opcode::JobBufferSidebandRa: {
          BatchJobSidebandBufferRa *job = (BatchJobSidebandBufferRa *)jobPtr;
          ORBIS_LOG_ERROR(__FUNCTION__, request, "BatchJobSidebandBufferRa",
                          job->opcode, job->sidebandSize, job->pSideband);
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
          stream->inputSize = runJob.inputSize;
          stream->outputSize = runJob.outputSize;
        }
      } else if (!runJob.control) {
        orbis::uint32_t maxChannels =
            instance.maxChannels == AJM_CHANNEL_DEFAULT ? 2
                                                        : instance.maxChannels;
        AJMSidebandResult *result =
            reinterpret_cast<AJMSidebandResult *>(runJob.pSideband);
        result->result = 0;
        result->codecResult = 0;

        orbis::uint32_t inputReaded = 0;
        orbis::uint32_t outputWritten = 0;
        orbis::uint32_t framesProcessed = 0;
        orbis::uint32_t channels = 0;
        orbis::uint32_t sampleRate = 0;
        if (runJob.inputSize != 0 && runJob.outputSize != 0) {
          while (inputReaded < runJob.inputSize &&
                 outputWritten < runJob.outputSize) {
            // TODO: initialize if not
            if (instance.at9.frameSamples == 0 &&
                instance.codec == AJM_CODEC_At9) {
              break;
            }
            if (instance.codec == AJM_CODEC_At9) {
              orbis::uint32_t outputChannels =
                  instance.at9.inputChannels > maxChannels
                      ? maxChannels
                      : instance.at9.inputChannels;
              orbis::int32_t outputBufferSize = av_samples_get_buffer_size(
                  nullptr, outputChannels, instance.at9.frameSamples,
                  ajmToAvFormat(instance.resampler ? AJM_FORMAT_S16
                                                   : instance.outputFormat),
                  0);

              orbis::uint8_t *tempBuffer =
                  instance.resampler ? (uint8_t *)av_malloc(outputBufferSize)
                                     : reinterpret_cast<orbis::uint8_t *>(
                                           runJob.pOutput + outputWritten);
              orbis::int32_t bytesUsed = 0;
              int err =
                  Atrac9Decode(instance.at9.handle, runJob.pInput + inputReaded,
                               tempBuffer, kAtrac9FormatS16, &bytesUsed);
              if (err != ERR_SUCCESS) {
                ORBIS_LOG_FATAL("Could not decode frame", err);
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

              ORBIS_LOG_TODO("used size", bytesUsed,
                             instance.at9.estimatedSizeUsed);
              // TODO: possible memory leak because "genius" code to avoiding
              // memory copying
              framesProcessed += 1;
              if (instance.gaplessSkipSamples > 0 ||
                  instance.gaplessTotalSamples > 0) {
                if (instance.gaplessSkipSamples >
                    instance.gaplessTotalSkippedSamples) {
                  instance.gaplessTotalSkippedSamples +=
                      instance.at9.frameSamples;
                  inputReaded += instance.at9.estimatedSizeUsed;
                  ORBIS_LOG_TODO("skip frame",
                                 instance.gaplessTotalSkippedSamples);
                  continue;
                } else if (instance.processedSamples >
                           instance.gaplessTotalSamples) {
                  instance.gaplessTotalSkippedSamples +=
                      instance.at9.frameSamples;
                  inputReaded += instance.at9.estimatedSizeUsed;
                  ORBIS_LOG_TODO(
                      "skip output", instance.gaplessTotalSkippedSamples,
                      instance.processedSamples, instance.gaplessTotalSamples);
                  continue;
                }
              }
              if (instance.resampler) {
                auto outputBuffer = reinterpret_cast<orbis::uint8_t *>(
                    runJob.pOutput + outputWritten);

                int nb_samples =
                    swr_convert(instance.resampler, &outputBuffer,
                                instance.at9.frameSamples, &tempBuffer,
                                instance.at9.frameSamples);
                if (nb_samples < 0) {
                  ORBIS_LOG_FATAL("Error while resampling", nb_samples);
                  std::abort();
                }
                av_freep(&tempBuffer);
              }
              channels = instance.at9.inputChannels;
              sampleRate = instance.at9.sampleRate;
              inputReaded += instance.at9.estimatedSizeUsed;
              // outputWritten += std::max((orbis::uint32_t)outputBufferSize,
              //                           runJob.outputSize);
              outputWritten += outputBufferSize;
              instance.processedSamples += instance.at9.frameSamples;
            } else if (instance.codec == AJM_CODEC_MP3) {
              ORBIS_LOG_FATAL("Pre get mp3 data size info", runJob.inputSize,
                              runJob.outputSize, runJob.sidebandSize,
                              runJob.flags);
              auto realInputSize =
                  get_mp3_data_size((uint8_t *)(runJob.pInput + inputReaded));
              if (realInputSize == 0) {
                realInputSize = runJob.inputSize;
              } else {
                realInputSize = std::min(realInputSize, runJob.inputSize);
              }

              if (inputReaded + realInputSize > runJob.inputSize) {
                break;
              }

              // rx::hexdump(
              //     {(std::byte *)(runJob.pInput + inputReaded),
              //     realInputSize});

              AVPacket *pkt = av_packet_alloc();
              rx::atScopeExit _free_pkt([&] { av_packet_free(&pkt); });
              AVFrame *frame = av_frame_alloc();
              rx::atScopeExit _free_frame([&] { av_frame_free(&frame); });
              pkt->data = (orbis::uint8_t *)(runJob.pInput + inputReaded);
              pkt->size = realInputSize;
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

              if (instance.gaplessSkipSamples > 0 ||
                  instance.gaplessTotalSamples > 0) {
                if (instance.gaplessSkipSamples >
                    instance.gaplessTotalSkippedSamples) {
                  instance.gaplessTotalSkippedSamples += frame->nb_samples;
                  inputReaded += realInputSize;
                  ORBIS_LOG_TODO("skip frame",
                                 instance.gaplessTotalSkippedSamples);
                  break;
                } else if (instance.processedSamples >
                           instance.gaplessTotalSamples) {
                  instance.gaplessTotalSkippedSamples += frame->nb_samples;
                  inputReaded += realInputSize;
                  ORBIS_LOG_TODO(
                      "skip output", instance.gaplessTotalSkippedSamples,
                      instance.processedSamples, instance.gaplessTotalSamples);
                  break;
                }
              }

              auto resampler = swr_alloc();
              rx::atScopeExit _free_resampler([&] { swr_free(&resampler); });
              if (!resampler) {
                ORBIS_LOG_FATAL("Could not allocate resampler context");
                std::abort();
              }

              orbis::uint32_t outputChannels =
                  (orbis::uint32_t)frame->ch_layout.nb_channels > maxChannels
                      ? maxChannels
                      : frame->ch_layout.nb_channels;

              AVChannelLayout inputChLayout;
              av_channel_layout_default(&inputChLayout,
                                        frame->ch_layout.nb_channels);

              AVChannelLayout outputChLayout;
              av_channel_layout_default(&outputChLayout, outputChannels);

              av_opt_set_chlayout(resampler, "in_chlayout", &inputChLayout, 0);
              av_opt_set_chlayout(resampler, "out_chlayout", &outputChLayout,
                                  0);
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

              uint8_t *outputBuffer = NULL;
              rx::atScopeExit _free_outputBuffer(
                  [&] { av_freep(&outputBuffer); });
              int outputBufferSize = av_samples_alloc(
                  &outputBuffer, NULL, frame->ch_layout.nb_channels,
                  frame->nb_samples, ajmToAvFormat(instance.outputFormat), 0);
              if (outputBufferSize < 0) {
                ORBIS_LOG_FATAL("Could not allocate output buffer");
                std::abort();
              }
              ORBIS_LOG_TODO("output buffer info", frame->ch_layout.nb_channels,
                             frame->nb_samples, (int32_t)instance.outputFormat,
                             outputBufferSize,
                             instance.gaplessTotalSkippedSamples,
                             instance.processedSamples);

              if (outputWritten + outputBufferSize > runJob.outputSize) {
                ORBIS_LOG_TODO("overwriting", outputWritten, outputBufferSize,
                               outputWritten + outputBufferSize,
                               runJob.outputSize);
                break;
              }

              int nb_samples =
                  swr_convert(resampler, &outputBuffer, frame->nb_samples,
                              (const uint8_t **)frame->data, frame->nb_samples);
              if (nb_samples < 0) {
                ORBIS_LOG_FATAL("Error while converting");
                std::abort();
              }

              memcpy(runJob.pOutput + outputWritten, outputBuffer,
                     outputBufferSize);
              channels = frame->ch_layout.nb_channels;
              sampleRate = frame->sample_rate;
              inputReaded += realInputSize;
              outputWritten += outputBufferSize;
              instance.processedSamples += frame->nb_samples;
              framesProcessed += 1;
              // av_freep(&outputBuffer);
              // swr_free(&resampler);
              // av_frame_free(&frame);
              // av_packet_free(&pkt);
            } else if (instance.codec == AJM_CODEC_AAC) {
              AVPacket *pkt = av_packet_alloc();
              rx::atScopeExit _free_pkt([&] { av_packet_free(&pkt); });
              AVFrame *frame = av_frame_alloc();
              rx::atScopeExit _free_frame([&] { av_frame_free(&frame); });
              pkt->data = (uint8_t *)runJob.pInput + inputReaded;
              pkt->size = runJob.inputSize;

              // HACK: to avoid writing a bunch of useless calls
              // we simply call this method directly (but it can be very
              // unstable)
              int gotFrame;
              int len =
                  ffcodec(instance.codecCtx->codec)
                      ->cb.decode(instance.codecCtx, frame, &gotFrame, pkt);

              orbis::uint32_t outputChannels =
                  (orbis::uint32_t)frame->ch_layout.nb_channels > maxChannels
                      ? maxChannels
                      : frame->ch_layout.nb_channels;

              ORBIS_LOG_TODO("aac decode", len, gotFrame,
                             frame->ch_layout.nb_channels, frame->sample_rate,
                             instance.aac.sampleRate, outputChannels,
                             (orbis::uint32_t)instance.maxChannels);

              if (instance.gaplessSkipSamples > 0 ||
                  instance.gaplessTotalSamples > 0) {
                if (instance.gaplessSkipSamples >
                    instance.gaplessTotalSkippedSamples) {
                  instance.gaplessTotalSkippedSamples += frame->nb_samples;
                  inputReaded += len;
                  ORBIS_LOG_TODO("skip frame",
                                 instance.gaplessTotalSkippedSamples);
                  break;
                } else if (instance.processedSamples >
                           instance.gaplessTotalSamples) {
                  instance.gaplessTotalSkippedSamples += frame->nb_samples;
                  inputReaded += len;
                  ORBIS_LOG_TODO(
                      "skip output", instance.gaplessTotalSkippedSamples,
                      instance.processedSamples, instance.gaplessTotalSamples);
                  break;
                }
              }

              auto resampler = swr_alloc();
              rx::atScopeExit _free_resampler([&] { swr_free(&resampler); });
              if (!resampler) {
                ORBIS_LOG_FATAL("Could not allocate resampler context");
                std::abort();
              }

              AVChannelLayout inputChLayout;
              av_channel_layout_default(&inputChLayout,
                                        frame->ch_layout.nb_channels);

              AVChannelLayout outputChLayout;
              av_channel_layout_default(&outputChLayout, outputChannels);

              av_opt_set_chlayout(resampler, "in_chlayout", &inputChLayout, 0);
              av_opt_set_chlayout(resampler, "out_chlayout", &outputChLayout,
                                  0);
              av_opt_set_int(resampler, "in_sample_rate",
                             instance.aac.sampleRate, 0);
              av_opt_set_int(resampler, "out_sample_rate",
                             instance.aac.sampleRate, 0);
              av_opt_set_sample_fmt(resampler, "in_sample_fmt",
                                    ajmToAvFormat(AJM_FORMAT_FLOAT), 0);
              av_opt_set_sample_fmt(resampler, "out_sample_fmt",
                                    ajmToAvFormat(instance.outputFormat), 0);
              if (swr_init(resampler) < 0) {
                ORBIS_LOG_FATAL("Failed to initialize the resampling context");
                std::abort();
              }

              uint8_t *outputBuffer = NULL;
              rx::atScopeExit _free_outputBuffer(
                  [&] { av_freep(&outputBuffer); });
              int outputBufferSize = av_samples_alloc(
                  &outputBuffer, NULL, outputChannels, frame->nb_samples,
                  ajmToAvFormat(instance.outputFormat), 0);
              if (outputBufferSize < 0) {
                ORBIS_LOG_FATAL("Could not allocate output buffer");
                std::abort();
              }

              int nb_samples =
                  swr_convert(resampler, &outputBuffer, frame->nb_samples,
                              frame->extended_data, frame->nb_samples);
              if (nb_samples < 0) {
                ORBIS_LOG_FATAL("Error while converting");
                std::abort();
              }

              memcpy(runJob.pOutput + outputWritten, outputBuffer,
                     outputBufferSize);
              channels = frame->ch_layout.nb_channels;
              sampleRate = frame->sample_rate;
              inputReaded += len;
              outputWritten += outputBufferSize;
              framesProcessed += 1;
              instance.processedSamples += frame->nb_samples;
              // av_frame_free(&frame);
              // av_packet_free(&pkt);
              // swr_free(&resampler);
            }
            if (!(runJob.flags & RUN_MULTIPLE_FRAMES)) {
              break;
            }
          }
        }

        orbis::int64_t currentSize = sizeof(AJMSidebandResult);

        if (runJob.flags & SIDEBAND_STREAM) {
          ORBIS_LOG_TODO("SIDEBAND_STREAM", currentSize, inputReaded,
                         outputWritten, instance.processedSamples);
          AJMSidebandStream *stream = reinterpret_cast<AJMSidebandStream *>(
              runJob.pSideband + currentSize);
          stream->inputSize = inputReaded;
          stream->outputSize = outputWritten;
          stream->decodedSamples = instance.processedSamples;
          currentSize += sizeof(AJMSidebandStream);
        }

        if (runJob.flags & SIDEBAND_FORMAT) {
          ORBIS_LOG_TODO("SIDEBAND_FORMAT", currentSize);
          AJMSidebandFormat *format = reinterpret_cast<AJMSidebandFormat *>(
              runJob.pSideband + currentSize);
          format->channels = AJMChannels(channels);
          format->sampleRate = sampleRate;
          format->sampleFormat = AJM_FORMAT_FLOAT;
          currentSize += sizeof(AJMSidebandFormat);
        }

        if (runJob.flags & SIDEBAND_GAPLESS_DECODE) {
          ORBIS_LOG_TODO("SIDEBAND_GAPLESS_DECODE", currentSize);
          AJMSidebandGaplessDecode *gapless =
              reinterpret_cast<AJMSidebandGaplessDecode *>(runJob.pSideband +
                                                           currentSize);
          gapless->skipSamples = instance.gaplessSkipSamples;
          gapless->totalSamples = instance.gaplessTotalSamples;
          gapless->totalSkippedSamples = instance.gaplessTotalSkippedSamples;
          currentSize += sizeof(AJMSidebandGaplessDecode);
        }

        if (runJob.flags & RUN_GET_CODEC_INFO) {
          ORBIS_LOG_TODO("RUN_GET_CODEC_INFO");
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
          ORBIS_LOG_TODO("RUN_MULTIPLE_FRAMES", currentSize);
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
    ORBIS_LOG_ERROR(__FUNCTION__, request, args->unk0, args->unk1,
                    args->batchId, args->timeout, args->batchError);
    thread->where();
  } else {
    ORBIS_LOG_FATAL("Unhandled AJM ioctl", request);
    thread->where();
  }
  return {};
}

static const orbis::FileOps fileOps = {
    .ioctl = ajm_ioctl,
};

struct AjmDevice : IoDevice {
  orbis::ErrorCode open(orbis::Ref<orbis::File> *file, const char *path,
                        std::uint32_t flags, std::uint32_t mode,
                        orbis::Thread *thread) override {
    auto newFile = orbis::knew<AjmFile>();
    newFile->ops = &fileOps;
    newFile->device = this;

    *file = newFile;
    return {};
  }
};

IoDevice *createAjmCharacterDevice() { return orbis::knew<AjmDevice>(); }
