#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <orbis/evf.hpp>
#include <orbis/utils/Rc.hpp>
#include <thread>
#include <vector>

struct AudioOutChannelInfo {
  std::int32_t port{};
  std::int32_t idControl{};
  std::int32_t channel{};
  orbis::Ref<orbis::EventFlag> evf;
};

struct AudioOutParams {
  std::uint64_t control{};
  std::uint32_t formatChannels{};
  float unk0{};
  float unk1{};
  float unk2{};
  float unk3{};
  float unk4{};
  float unk5{};
  float unk6{};
  float unk7{};
  std::uint32_t formatIsFloat{};
  std::uint64_t freq{};
  std::uint32_t formatIsStd{};
  std::uint32_t seek{};
  std::uint32_t seekPart{};
  std::uint64_t unk8{};
  std::uint32_t port{};
  std::uint32_t unk9{};
  std::uint64_t unk10{};
  std::uint32_t sampleLength{};
};

struct AudioOut : orbis::RcBase {
  std::mutex thrMtx;
  std::mutex soxMtx;
  std::vector<std::thread> threads;
  AudioOutChannelInfo channelInfo;
  std::atomic<bool> exit{false};

  AudioOut();
  ~AudioOut();

  void start();

private:
  void channelEntry(AudioOutChannelInfo info);
};
