#pragma once
#include "Registers.hpp"
#include "Scheduler.hpp"
#include "orbis/utils/SharedMutex.hpp"

#include <cstdint>
#include <vulkan/vulkan_core.h>

namespace amdgpu {
struct Device;

struct Ring {
  int vmId = -1;
  int indirectLevel = -1;
  std::uint32_t *doorbell{};
  std::uint32_t *base{};
  std::uint64_t size{};
  std::uint32_t *rptr{};
  std::uint32_t *wptr{};
  std::uint32_t *rptrReportLocation{};

  static Ring createFromRange(int vmId, std::uint32_t *base, std::uint64_t size,
                              int indirectLevel = 0,
                              std::uint32_t *doorbell = nullptr) {
    Ring result;
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
  static constexpr auto kRingsPerQueue = 2;
  static constexpr auto kQueueCount = 8;
  Device *device;
  Scheduler scheduler;

  using CommandHandler = bool (ComputePipe::*)(Ring &);
  CommandHandler commandHandlers[255];
  orbis::shared_mutex queueMtx[kQueueCount];
  int index;
  int currentQueueId;
  Ring queues[kRingsPerQueue][kQueueCount];
  std::uint64_t drawIndexIndirPatchBase = 0;

  ComputePipe(int index);

  bool processAllRings();
  bool processRing(Ring &ring);
  void setIndirectRing(int queueId, int level, Ring ring);
  void mapQueue(int queueId, Ring ring,
                std::unique_lock<orbis::shared_mutex> &lock);
  void waitForIdle(int queueId, std::unique_lock<orbis::shared_mutex> &lock);
  void submit(int queueId, std::uint32_t offset);

  std::unique_lock<orbis::shared_mutex> lockQueue(int queueId) {
    return std::unique_lock<orbis::shared_mutex>(queueMtx[queueId]);
  }

  bool setShReg(Ring &ring);
  bool dispatchDirect(Ring &ring);
  bool dispatchIndirect(Ring &ring);
  bool releaseMem(Ring &ring);
  bool waitRegMem(Ring &ring);
  bool writeData(Ring &ring);
  bool indirectBuffer(Ring &ring);
  bool acquireMem(Ring &ring);
  bool unknownPacket(Ring &ring);
  bool handleNop(Ring &ring);

  std::uint32_t *getMmRegister(Ring &ring, std::uint32_t dwAddress);
};

struct EopFlipRequest {
  std::uint32_t pid;
  int bufferIndex;
  std::uint64_t arg;
  std::uint64_t eopValue;
};

struct GraphicsPipe {
  static constexpr auto kEopFlipRequestMax = 0x10;
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

  Ring deQueues[3];
  Ring ceQueue;

  orbis::shared_mutex eopFlipMtx;
  std::uint32_t eopFlipRequestCount{0};
  EopFlipRequest eopFlipRequests[kEopFlipRequestMax];

  using CommandHandler = bool (GraphicsPipe::*)(Ring &);
  CommandHandler commandHandlers[4][255];

  GraphicsPipe(int index);

  void setCeQueue(Ring ring);
  void setDeQueue(Ring ring, int indirectLevel);

  bool processAllRings();
  void processRing(Ring &ring);

  bool drawPreamble(Ring &ring);
  bool indexBufferSize(Ring &ring);
  bool handleNop(Ring &ring);
  bool contextControl(Ring &ring);
  bool acquireMem(Ring &ring);
  bool releaseMem(Ring &ring);
  bool dispatchDirect(Ring &ring);
  bool dispatchIndirect(Ring &ring);
  bool writeData(Ring &ring);
  bool memSemaphore(Ring &ring);
  bool waitRegMem(Ring &ring);
  bool indirectBufferConst(Ring &ring);
  bool indirectBuffer(Ring &ring);
  bool condWrite(Ring &ring);
  bool eventWrite(Ring &ring);
  bool eventWriteEop(Ring &ring);
  bool eventWriteEos(Ring &ring);
  bool dmaData(Ring &ring);
  bool setBase(Ring &ring);
  bool clearState(Ring &ring);
  bool setPredication(Ring &ring);
  bool drawIndirect(Ring &ring);
  bool drawIndexIndirect(Ring &ring);
  bool indexBase(Ring &ring);
  bool drawIndex2(Ring &ring);
  bool indexType(Ring &ring);
  bool drawIndexAuto(Ring &ring);
  bool numInstances(Ring &ring);
  bool drawIndexMultiAuto(Ring &ring);
  bool drawIndexOffset2(Ring &ring);
  bool pfpSyncMe(Ring &ring);
  bool setCeDeCounters(Ring &ring);
  bool waitOnCeCounter(Ring &ring);
  bool waitOnDeCounterDiff(Ring &ring);
  bool incrementCeCounter(Ring &ring);
  bool incrementDeCounter(Ring &ring);
  bool loadConstRam(Ring &ring);
  bool writeConstRam(Ring &ring);
  bool dumpConstRam(Ring &ring);
  bool setConfigReg(Ring &ring);
  bool setShReg(Ring &ring);
  bool setUConfigReg(Ring &ring);
  bool setContextReg(Ring &ring);
  bool mapQueues(Ring &ring);
  bool unmapQueues(Ring &ring);

  bool unknownPacket(Ring &ring);
  bool switchBuffer(Ring &ring);

  std::uint32_t *getMmRegister(std::uint32_t dwAddress);
};

struct CommandPipe {
  Ring ring;
  Device *device;
  using CommandHandler = void (CommandPipe::*)(Ring &);
  CommandHandler commandHandlers[255];

  CommandPipe();

  void processAllRings();
  void processRing(Ring &ring);

  void mapProcess(Ring &ring);
  void mapMemory(Ring &ring);
  void unmapMemory(Ring &ring);
  void protectMemory(Ring &ring);
  void unmapProcess(Ring &ring);
  void flip(Ring &ring);

  void unknownPacket(Ring &ring);
};
} // namespace amdgpu