#pragma once
#include "Registers.hpp"
#include "Scheduler.hpp"

#include <cstdint>
#include <vulkan/vulkan_core.h>

namespace amdgpu {
class Device;

struct Queue {
  int vmId = -1;
  int indirectLevel = -1;
  std::uint32_t *doorbell{};
  std::uint32_t *base{};
  std::uint64_t size{};
  std::uint32_t *rptr{};
  std::uint32_t *wptr{};

  static Queue createFromRange(int vmId, std::uint32_t *base,
                               std::uint64_t size, int indirectLevel = 0,
                               std::uint32_t *doorbell = nullptr) {
    Queue result;
    result.vmId = vmId;
    result.indirectLevel = indirectLevel;
    result.doorbell = doorbell;
    result.base = base;
    result.size = size;
    result.rptr = base;
    result.wptr = base + size;
    return result;
  }
};

struct ComputePipe {
  Device *device;
  Scheduler scheduler;

  using CommandHandler = bool (ComputePipe::*)(Queue &);
  CommandHandler commandHandlers[255];
  Queue queues[8];
  Registers::ComputeConfig computeConfig;

  ComputePipe(int index);

  bool processAllRings();
  void processRing(Queue &queue);
  void mapQueue(int queueId, Queue queue);

  bool setShReg(Queue &queue);
  bool unknownPacket(Queue &queue);
  bool handleNop(Queue &queue);
};

struct GraphicsPipe {
  Device *device;
  Scheduler scheduler;

  std::uint64_t ceCounter = 0;
  std::uint64_t deCounter = 0;
  std::uint64_t displayListPatchBase = 0;
  std::uint64_t drawIndexIndirPatchBase = 0;
  std::uint64_t gdsPartitionBases[2]{};
  std::uint64_t cePartitionBases[2]{};
  std::uint64_t vgtIndexBase = 0;
  std::uint32_t vgtIndexBufferSize = 0;

  std::uint32_t constantMemory[(48 * 1024) / sizeof(std::uint32_t)]{};

  Registers::ShaderConfig sh;
  Registers::Context context;
  Registers::UConfig uConfig;

  Queue deQueues[3];
  Queue ceQueue;

  using CommandHandler = bool (GraphicsPipe::*)(Queue &);
  CommandHandler commandHandlers[3][255];

  GraphicsPipe(int index);

  void setCeQueue(Queue queue);
  void setDeQueue(Queue queue, int ring);

  bool processAllRings();
  void processRing(Queue &queue);

  bool drawPreamble(Queue &queue);
  bool indexBufferSize(Queue &queue);
  bool handleNop(Queue &queue);
  bool contextControl(Queue &queue);
  bool acquireMem(Queue &queue);
  bool releaseMem(Queue &queue);
  bool dispatchDirect(Queue &queue);
  bool dispatchIndirect(Queue &queue);
  bool writeData(Queue &queue);
  bool memSemaphore(Queue &queue);
  bool waitRegMem(Queue &queue);
  bool indirectBuffer(Queue &queue);
  bool condWrite(Queue &queue);
  bool eventWrite(Queue &queue);
  bool eventWriteEop(Queue &queue);
  bool eventWriteEos(Queue &queue);
  bool dmaData(Queue &queue);
  bool setBase(Queue &queue);
  bool clearState(Queue &queue);
  bool setPredication(Queue &queue);
  bool drawIndirect(Queue &queue);
  bool drawIndexIndirect(Queue &queue);
  bool indexBase(Queue &queue);
  bool drawIndex2(Queue &queue);
  bool indexType(Queue &queue);
  bool drawIndexAuto(Queue &queue);
  bool numInstances(Queue &queue);
  bool drawIndexMultiAuto(Queue &queue);
  bool drawIndexOffset2(Queue &queue);
  bool pfpSyncMe(Queue &queue);
  bool setCeDeCounters(Queue &queue);
  bool waitOnCeCounter(Queue &queue);
  bool waitOnDeCounterDiff(Queue &queue);
  bool incrementCeCounter(Queue &queue);
  bool incrementDeCounter(Queue &queue);
  bool loadConstRam(Queue &queue);
  bool writeConstRam(Queue &queue);
  bool dumpConstRam(Queue &queue);
  bool setConfigReg(Queue &queue);
  bool setShReg(Queue &queue);
  bool setUConfigReg(Queue &queue);
  bool setContextReg(Queue &queue);

  bool unknownPacket(Queue &queue);

  std::uint32_t *getMmRegister(std::uint32_t dwAddress);
};
} // namespace amdgpu