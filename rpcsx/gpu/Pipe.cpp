#include "Pipe.hpp"
#include "Device.hpp"
#include "Registers.hpp"
#include "Renderer.hpp"
#include "gnm/mmio.hpp"
#include "gnm/pm4.hpp"
#include "orbis/KernelContext.hpp"
#include "vk.hpp"
#include <bit>
#include <cstdio>
#include <mutex>
#include <print>
#include <rx/bits.hpp>
#include <rx/die.hpp>
#include <vulkan/vulkan_core.h>

using namespace amdgpu;

enum GraphicsCoreEvent {
  kGcEventCompute0RelMem = 0x00,
  kGcEventCompute1RelMem = 0x01,
  kGcEventCompute2RelMem = 0x02,
  kGcEventCompute3RelMem = 0x03,
  kGcEventCompute4RelMem = 0x04,
  kGcEventCompute5RelMem = 0x05,
  kGcEventCompute6RelMem = 0x06,
  kGcEventGfxEop = 0x40,
  kGcEventClockSet = 0x84,
};

static Scheduler createGfxScheduler(int index) {
  auto queue = vk::context->presentQueue;
  auto family = vk::context->presentQueueFamily;

  if (index != 0) {
    for (auto [otherQueue, otherFamily] : vk::context->graphicsQueues) {
      if (family != otherFamily) {
        queue = otherQueue;
        family = otherFamily;
      }
    }
  }

  return Scheduler{queue, family};
}

static Scheduler createComputeScheduler(int index) {
  auto &compQueues = vk::context->computeQueues;

  if (compQueues.empty()) {
    // Workaround for LLVM device
    return createGfxScheduler(index);
  }

  auto [queue, family] = compQueues[index % compQueues.size()];

  return Scheduler{queue, family};
}

static bool compare(int cmpFn, std::uint32_t poll, std::uint32_t mask,
                    std::uint32_t ref) {
  poll &= mask;
  ref &= mask;

  switch (cmpFn) {
  case 0:
    return true;
  case 1:
    return poll < ref;
  case 2:
    return poll <= ref;
  case 3:
    return poll == ref;
  case 4:
    return poll != ref;
  case 5:
    return poll >= ref;
  case 6:
    return poll > ref;
  }

  return false;
}

ComputePipe::ComputePipe(int index)
    : scheduler(createComputeScheduler(index)), index(index) {
  for (auto &handler : commandHandlers) {
    handler = &ComputePipe::unknownPacket;
  }

  commandHandlers[gnm::IT_NOP] = &ComputePipe::handleNop;
  commandHandlers[gnm::IT_SET_SH_REG] = &ComputePipe::setShReg;
  commandHandlers[gnm::IT_DISPATCH_DIRECT] = &ComputePipe::dispatchDirect;
  commandHandlers[gnm::IT_DISPATCH_INDIRECT] = &ComputePipe::dispatchIndirect;
  commandHandlers[gnm::IT_RELEASE_MEM] = &ComputePipe::releaseMem;
  commandHandlers[gnm::IT_WAIT_REG_MEM] = &ComputePipe::waitRegMem;
  commandHandlers[gnm::IT_WRITE_DATA] = &ComputePipe::writeData;
  commandHandlers[gnm::IT_INDIRECT_BUFFER] = &ComputePipe::indirectBuffer;
  commandHandlers[gnm::IT_ACQUIRE_MEM] = &ComputePipe::acquireMem;
}

bool ComputePipe::processAllRings() {
  bool allProcessed = true;

  for (auto &queue : queues) {
    std::lock_guard lock(queueMtx[&queue - queues]);

    for (auto &ring : queue) {
      currentQueueId = &ring - queue;
      if (!processRing(ring)) {
        allProcessed = false;
      }
    }
  }

  return allProcessed;
}

bool ComputePipe::processRing(Ring &ring) {
  if (ring.size == 0) {
    return true;
  }

  while (true) {
    if (ring.rptrReportLocation != nullptr) {
      // FIXME: verify
      ring.rptr = ring.base + *ring.rptrReportLocation;
    }

    while (ring.rptr != ring.wptr) {
      if (ring.rptr >= ring.base + ring.size) {
        ring.rptr = ring.base;
        continue;
      }

      auto header = *ring.rptr;
      auto type = rx::getBits(header, 31, 30);

      if (type == 3) {
        auto op = rx::getBits(header, 15, 8);
        auto len = rx::getBits(header, 29, 16) + 2;

        // std::fprintf(stderr, "queue %d: %s\n", ring.indirectLevel,
        //              gnm::pm4OpcodeToString(op));

        if (op == gnm::IT_COND_EXEC) {
          rx::die("unimplemented COND_EXEC");
        }

        auto handler = commandHandlers[op];
        if (!(this->*handler)(ring)) {
          if (ring.rptrReportLocation != nullptr) {
            *ring.rptrReportLocation = ring.rptr - ring.base;
          }
          return false;
        }

        ring.rptr += len;
        continue;
      }

      if (type == 2) {
        ++ring.rptr;
        continue;
      }

      rx::die("unexpected pm4 packet type %u", type);
    }

    if (ring.rptrReportLocation != nullptr) {
      *ring.rptrReportLocation = ring.rptr - ring.base;
    }

    break;
  }

  return true;
}

void ComputePipe::setIndirectRing(int queueId, int indirectLevel, Ring ring) {
  if (indirectLevel != 1) {
    rx::die("unexpected compute indirect ring indirect level %d",
            ring.indirectLevel);
  }

  ring.indirectLevel = indirectLevel;
  std::println("mapQueue: {}, {}, {}", (void *)ring.base, (void *)ring.wptr,
               ring.size);

  queues[1 - ring.indirectLevel][queueId] = ring;
}

void ComputePipe::mapQueue(int queueId, Ring ring,
                           std::unique_lock<orbis::shared_mutex> &lock) {
  if (ring.indirectLevel < 0 || ring.indirectLevel > 1) {
    rx::die("unexpected compute ring indirect level %d", ring.indirectLevel);
  }

  if (ring.indirectLevel == 0) {
    waitForIdle(queueId, lock);
  }

  std::println("mapQueue: {}, {}, {}, {}", (void *)ring.base, (void *)ring.wptr,
               ring.size, (void *)ring.doorbell);

  queues[1 - ring.indirectLevel][queueId] = ring;
}

void ComputePipe::waitForIdle(int queueId,
                              std::unique_lock<orbis::shared_mutex> &lock) {
  auto &ring = queues[1][queueId];

  while (true) {
    if (ring.size == 0) {
      return;
    }

    if (ring.rptr == ring.wptr) {
      return;
    }

    lock.unlock();
    std::this_thread::sleep_for(std::chrono::microseconds(10));
    lock.lock();
  }
}

void ComputePipe::submit(int queueId, std::uint32_t offset) {
  auto &ring = queues[1][queueId];
  ring.wptr = ring.base + offset;
}

bool ComputePipe::setShReg(Ring &ring) {
  auto len = rx::getBits(ring.rptr[0], 29, 16);
  auto offset = ring.rptr[1] & 0xffff;
  auto index = ring.rptr[1] >> 26;
  auto data = ring.rptr + 2;

  if (Registers::ShaderConfig::kMmioOffset + offset <
      Registers::ComputeConfig::kMmioOffset) {
    rx::die(
        "unexpected compute pipe offset %x %s", offset,
        gnm::mmio::registerName(Registers::ShaderConfig::kMmioOffset + offset));
  }

  offset -= Registers::ComputeConfig::kMmioOffset -
            Registers::ShaderConfig::kMmioOffset;

  rx::dieIf(
      (offset + len) * sizeof(std::uint32_t) > sizeof(Registers::ComputeConfig),
      "out of compute regs, offset: %x, count %u, %s\n", offset, len,
      gnm::mmio::registerName(Registers::ShaderConfig::kMmioOffset + offset));

  for (std::size_t i = 0; i < len; ++i) {
    std::fprintf(stderr, "writing to %s value %x\n",
                 gnm::mmio::registerName(Registers::ShaderConfig::kMmioOffset +
                                         offset + i),
                 data[i]);
  }

  std::memcpy(ring.doorbell + offset, data, sizeof(std::uint32_t) * len);

  return true;
}

bool ComputePipe::dispatchDirect(Ring &ring) {
  auto config = std::bit_cast<Registers::ComputeConfig *>(ring.doorbell);
  auto dimX = ring.rptr[1];
  auto dimY = ring.rptr[2];
  auto dimZ = ring.rptr[3];
  auto dispatchInitiator = ring.rptr[4];
  config->computeDispatchInitiator = dispatchInitiator;

  amdgpu::dispatch(device->caches[ring.vmId], scheduler, *config, dimX, dimY,
                   dimZ);
  return true;
}

bool ComputePipe::dispatchIndirect(Ring &ring) {
  auto config = std::bit_cast<Registers::ComputeConfig *>(ring.doorbell);
  auto offset = ring.rptr[1];
  auto dispatchInitiator = ring.rptr[2];

  config->computeDispatchInitiator = dispatchInitiator;
  auto buffer = RemoteMemory{ring.vmId}.getPointer<std::uint32_t>(
      drawIndexIndirPatchBase + offset);

  auto dimX = buffer[0];
  auto dimY = buffer[1];
  auto dimZ = buffer[2];

  amdgpu::dispatch(device->caches[ring.vmId], scheduler, *config, dimX, dimY,
                   dimZ);
  return true;
}

bool ComputePipe::releaseMem(Ring &ring) {
  auto eventCntl = ring.rptr[1];
  auto dataCntl = ring.rptr[2];
  auto addressLo = ring.rptr[3] & ~3;
  auto addressHi = ring.rptr[4] & ((1 << 16) - 1);
  auto dataLo = ring.rptr[5];
  auto dataHi = ring.rptr[6];

  auto eventIndex = rx::getBits(eventCntl, 11, 8);
  auto eventType = rx::getBits(eventCntl, 5, 0);
  auto dataSel = rx::getBits(dataCntl, 31, 29);
  auto intSel = rx::getBits(dataCntl, 25, 24);

  auto address = addressLo | (static_cast<std::uint64_t>(addressHi) << 32);
  auto pointer = RemoteMemory{ring.vmId}.getPointer<std::uint64_t>(address);

  switch (dataSel) {
  case 0: // none
    break;
  case 1: // 32 bit, low
    *reinterpret_cast<std::uint32_t *>(pointer) = dataLo;
    break;
  case 2: // 64 bit
    *pointer = dataLo | (static_cast<std::uint64_t>(dataHi) << 32);
    break;
  case 3: // 64 bit, global GPU clock
    *pointer = std::chrono::duration_cast<std::chrono::nanoseconds>(
                   std::chrono::system_clock::now().time_since_epoch())
                   .count();
    break;
  case 4: // 64 bit, perf counter
    *pointer = std::chrono::duration_cast<std::chrono::nanoseconds>(
                   std::chrono::steady_clock::now().time_since_epoch())
                   .count();
    break;

  default:
    rx::die("unimplemented event release mem data %#x", dataSel);
  }

  if (intSel) {
    orbis::g_context.deviceEventEmitter->emit(orbis::kEvFiltGraphicsCore, 0,
                                              kGcEventCompute0RelMem + index);
  }

  return true;
}

bool ComputePipe::waitRegMem(Ring &ring) {
  auto engine = rx::getBit(ring.rptr[1], 8);
  auto memSpace = rx::getBit(ring.rptr[1], 4);
  auto function = rx::getBits(ring.rptr[1], 2, 0);
  auto pollAddressLo = ring.rptr[2];
  auto pollAddressHi = ring.rptr[3] & ((1 << 16) - 1);
  auto reference = ring.rptr[4];
  auto mask = ring.rptr[5];
  auto pollInterval = ring.rptr[6];

  std::uint32_t pollData;

  if (memSpace == 0) {
    pollData = *getMmRegister(ring, pollAddressLo & ((1 << 16) - 1));
  } else {
    auto pollAddress = (pollAddressLo & ~3) |
                       (static_cast<std::uint64_t>(pollAddressHi) << 32);
    pollData = *RemoteMemory{ring.vmId}.getPointer<std::uint32_t>(pollAddress);
  }

  return compare(function, pollData, mask, reference);
}

bool ComputePipe::writeData(Ring &ring) {
  auto len = rx::getBits(ring.rptr[0], 29, 16) - 1;
  auto control = ring.rptr[1];
  auto dstAddressLo = ring.rptr[2];
  auto dstAddressHi = ring.rptr[3];
  auto data = ring.rptr + 4;

  auto engineSel = rx::getBits(control, 31, 30);
  auto wrConfirm = rx::getBit(control, 20);
  auto wrOneAddress = rx::getBit(control, 16);
  auto dstSel = rx::getBits(control, 11, 8);

  std::uint32_t *dstPointer = nullptr;

  switch (dstSel) {
  case 0: // memory mapped register
    dstPointer = getMmRegister(ring, dstAddressLo & ((1 << 16) - 1));
    break;

  case 1:   // memory sync
  case 2:   // TC L2
  case 5: { // memory async
    auto address =
        (dstAddressLo & ~3) | (static_cast<std::uint64_t>(dstAddressHi) << 32);
    dstPointer = RemoteMemory{ring.vmId}.getPointer<std::uint32_t>(address);
    break;
  }

  default:
    rx::die("unimplemented write data, dst sel = %#x", dstSel);
  }

  if (wrOneAddress) {
    for (std::uint32_t i = 0; i < len; ++i) {
      *dstPointer = data[i];
    }
  } else {
    std::memcpy(dstPointer, data, len * sizeof(std::uint32_t));
  }

  return true;
}

bool ComputePipe::indirectBuffer(Ring &ring) {
  rx::dieIf(ring.indirectLevel < 0, "unexpected indirect buffer from CP");

  auto addressLo = ring.rptr[1] & ~3;
  auto addressHi = ring.rptr[2] & ((1 << 8) - 1);
  int vmId = ring.rptr[3] >> 24;
  auto ibSize = ring.rptr[3] & ((1 << 20) - 1);
  auto address = addressLo | (static_cast<std::uint64_t>(addressHi) << 32);

  vmId = ring.vmId;

  auto rptr = RemoteMemory{vmId}.getPointer<std::uint32_t>(address);
  auto indirectRing = Ring::createFromRange(vmId, rptr, ibSize);
  indirectRing.doorbell = ring.doorbell;
  setIndirectRing(currentQueueId, ring.indirectLevel + 1, indirectRing);
  return true;
}

bool ComputePipe::acquireMem(Ring &ring) { return true; }

bool ComputePipe::unknownPacket(Ring &ring) {
  auto op = rx::getBits(ring.rptr[0], 15, 8);

  rx::die("unimplemented compute pm4 packet: %s, indirect level %u\n",
          gnm::pm4OpcodeToString(op), ring.indirectLevel);

  return true;
}

bool ComputePipe::handleNop(Ring &ring) { return true; }

std::uint32_t *ComputePipe::getMmRegister(Ring &ring, std::uint32_t dwAddress) {
  if (dwAddress >= Registers::ComputeConfig::kMmioOffset &&
      dwAddress <
          Registers::ComputeConfig::kMmioOffset +
              sizeof(Registers::ComputeConfig) / sizeof(std::uint32_t)) {
    return ring.doorbell + (dwAddress - Registers::ComputeConfig::kMmioOffset);
  }

  rx::die("unexpected memory mapped compute register address %x, %s", dwAddress,
          gnm::mmio::registerName(dwAddress));
}

GraphicsPipe::GraphicsPipe(int index) : scheduler(createGfxScheduler(index)) {
  for (auto &processorHandlers : commandHandlers) {
    for (auto &handler : processorHandlers) {
      handler = &GraphicsPipe::unknownPacket;
    }

    processorHandlers[gnm::IT_NOP] = &GraphicsPipe::handleNop;
  }

  auto &dataHandlers = commandHandlers[3];
  auto &deHandlers = commandHandlers[2];
  auto &mainHandlers = commandHandlers[1];
  auto &ceHandlers = commandHandlers[0];

  deHandlers[gnm::IT_SET_BASE] = &GraphicsPipe::setBase;
  deHandlers[gnm::IT_CLEAR_STATE] = &GraphicsPipe::clearState;

  deHandlers[gnm::IT_INDEX_BUFFER_SIZE] = &GraphicsPipe::indexBufferSize;
  deHandlers[gnm::IT_DISPATCH_DIRECT] = &GraphicsPipe::dispatchDirect;
  deHandlers[gnm::IT_DISPATCH_INDIRECT] = &GraphicsPipe::dispatchIndirect;

  // IT_ATOMIC_GDS
  // IT_OCCLUSION_QUERY
  deHandlers[gnm::IT_SET_PREDICATION] = &GraphicsPipe::setPredication;

  // IT_REG_RMW

  // IT_COND_EXEC
  // IT_PRED_EXEC

  deHandlers[gnm::IT_DRAW_INDIRECT] = &GraphicsPipe::drawIndirect;
  deHandlers[gnm::IT_DRAW_INDEX_INDIRECT] = &GraphicsPipe::drawIndexIndirect;
  deHandlers[gnm::IT_INDEX_BASE] = &GraphicsPipe::indexBase;
  deHandlers[gnm::IT_DRAW_INDEX_2] = &GraphicsPipe::drawIndex2;

  deHandlers[gnm::IT_CONTEXT_CONTROL] = &GraphicsPipe::contextControl;

  deHandlers[gnm::IT_INDEX_TYPE] = &GraphicsPipe::indexType;
  // IT_DRAW_INDIRECT_MULTI
  deHandlers[gnm::IT_DRAW_INDEX_AUTO] = &GraphicsPipe::drawIndexAuto;
  deHandlers[gnm::IT_NUM_INSTANCES] = &GraphicsPipe::numInstances;
  deHandlers[gnm::IT_DRAW_INDEX_MULTI_AUTO] = &GraphicsPipe::drawIndexMultiAuto;

  mainHandlers[gnm::IT_INDIRECT_BUFFER_CNST] =
      &GraphicsPipe::indirectBufferConst;
  // IT_STRMOUT_BUFFER_UPDATE

  deHandlers[gnm::IT_DRAW_INDEX_OFFSET_2] = &GraphicsPipe::drawIndexOffset2;
  deHandlers[gnm::IT_DRAW_PREAMBLE] = &GraphicsPipe::drawPreamble;

  deHandlers[gnm::IT_WRITE_DATA] = &GraphicsPipe::writeData;
  deHandlers[gnm::IT_MEM_SEMAPHORE] = &GraphicsPipe::memSemaphore;
  // IT_COPY_DW
  deHandlers[gnm::IT_WAIT_REG_MEM] = &GraphicsPipe::waitRegMem;
  deHandlers[gnm::IT_INDIRECT_BUFFER] = &GraphicsPipe::indirectBuffer;
  mainHandlers[gnm::IT_INDIRECT_BUFFER] = &GraphicsPipe::indirectBuffer;
  // IT_COPY_DATA
  deHandlers[gnm::IT_PFP_SYNC_ME] = &GraphicsPipe::pfpSyncMe;
  // IT_SURFACE_SYNC
  deHandlers[gnm::IT_COND_WRITE] = &GraphicsPipe::condWrite;
  deHandlers[gnm::IT_EVENT_WRITE] = &GraphicsPipe::eventWrite;
  deHandlers[gnm::IT_EVENT_WRITE_EOP] = &GraphicsPipe::eventWriteEop;
  deHandlers[gnm::IT_EVENT_WRITE_EOS] = &GraphicsPipe::eventWriteEos;
  deHandlers[gnm::IT_RELEASE_MEM] = &GraphicsPipe::releaseMem;
  // IT_PREAMBLE_CNTL
  deHandlers[gnm::IT_DMA_DATA] = &GraphicsPipe::dmaData;
  deHandlers[gnm::IT_ACQUIRE_MEM] = &GraphicsPipe::acquireMem;
  // IT_REWIND

  // IT_LOAD_UCONFIG_REG
  // IT_LOAD_SH_REG
  // IT_LOAD_CONFIG_REG
  // IT_LOAD_CONTEXT_REG
  deHandlers[gnm::IT_SET_CONFIG_REG] = &GraphicsPipe::setConfigReg;
  deHandlers[gnm::IT_SET_CONTEXT_REG] = &GraphicsPipe::setContextReg;
  // IT_SET_CONTEXT_REG_INDIRECT
  deHandlers[gnm::IT_SET_SH_REG] = &GraphicsPipe::setShReg;
  // IT_SET_SH_REG_OFFSET
  // IT_SET_QUEUE_REG
  deHandlers[gnm::IT_SET_UCONFIG_REG] = &GraphicsPipe::setUConfigReg;
  // IT_SCRATCH_RAM_WRITE
  // IT_SCRATCH_RAM_READ
  deHandlers[gnm::IT_INCREMENT_DE_COUNTER] = &GraphicsPipe::incrementDeCounter;
  deHandlers[gnm::IT_WAIT_ON_CE_COUNTER] = &GraphicsPipe::waitOnCeCounter;
  deHandlers[gnm::IT_SET_CE_DE_COUNTERS] = &GraphicsPipe::setCeDeCounters;
  // IT_WAIT_ON_AVAIL_BUFFER
  mainHandlers[gnm::IT_SWITCH_BUFFER] = &GraphicsPipe::switchBuffer;
  // IT_SET_RESOURCES
  mainHandlers[gnm::IT_MAP_PROCESS] = &GraphicsPipe::mapProcess;
  mainHandlers[gnm::IT_MAP_QUEUES] = &GraphicsPipe::mapQueues;
  mainHandlers[gnm::IT_UNMAP_QUEUES] = &GraphicsPipe::unmapQueues;
  mainHandlers[IT_MAP_MEMORY] = &GraphicsPipe::mapMemory;
  mainHandlers[IT_UNMAP_MEMORY] = &GraphicsPipe::unmapMemory;
  mainHandlers[IT_PROTECT_MEMORY] = &GraphicsPipe::protectMemory;
  mainHandlers[IT_UNMAP_PROCESS] = &GraphicsPipe::unmapProcess;
  // IT_QUERY_STATUS
  // IT_RUN_LIST
  // IT_DISPATCH_DRAW_PREAMBLE
  // IT_DISPATCH_DRAW

  ceHandlers[gnm::IT_WAIT_ON_DE_COUNTER_DIFF] =
      &GraphicsPipe::waitOnDeCounterDiff;
  ceHandlers[gnm::IT_INCREMENT_CE_COUNTER] = &GraphicsPipe::incrementCeCounter;
  ceHandlers[gnm::IT_LOAD_CONST_RAM] = &GraphicsPipe::loadConstRam;
  ceHandlers[gnm::IT_WRITE_CONST_RAM] = &GraphicsPipe::writeConstRam;
  ceHandlers[gnm::IT_DUMP_CONST_RAM] = &GraphicsPipe::dumpConstRam;

  mainHandlers[IT_FLIP] = &GraphicsPipe::flip;
}

void GraphicsPipe::setCeQueue(Ring ring) {
  ring.indirectLevel = -1;
  ceQueue = ring;
}

void GraphicsPipe::setDeQueue(Ring ring, int indirectLevel) {
  rx::dieIf(indirectLevel > 2, "out of indirect gfx rings, %u", indirectLevel);
  ring.indirectLevel = indirectLevel;
  deQueues[2 - indirectLevel] = ring;
}

std::uint32_t *GraphicsPipe::getMmRegister(std::uint32_t dwAddress) {
  // if (dwAddress >= Registers::Config::kMmioOffset &&
  //     dwAddress < Registers::Config::kMmioOffset +
  //     sizeof(Registers::Config) / sizeof(std::uint32_t)) {
  //   return reinterpret_cast<std::uint32_t *>(&config) + (dwAddress -
  //   Registers::Config::kMmioOffset);
  // }

  if (dwAddress >= Registers::ShaderConfig::kMmioOffset &&
      dwAddress < Registers::ShaderConfig::kMmioOffset +
                      sizeof(Registers::ShaderConfig) / sizeof(std::uint32_t)) {
    return reinterpret_cast<std::uint32_t *>(&sh) +
           (dwAddress - Registers::ShaderConfig::kMmioOffset);
  }

  if (dwAddress >= Registers::UConfig::kMmioOffset &&
      dwAddress < Registers::UConfig::kMmioOffset +
                      sizeof(Registers::UConfig) / sizeof(std::uint32_t)) {
    return reinterpret_cast<std::uint32_t *>(&uConfig) +
           (dwAddress - Registers::UConfig::kMmioOffset);
  }

  if (dwAddress >= Registers::Context::kMmioOffset &&
      dwAddress < Registers::Context::kMmioOffset +
                      sizeof(Registers::Context) / sizeof(std::uint32_t)) {
    return reinterpret_cast<std::uint32_t *>(&context) +
           (dwAddress - Registers::Context::kMmioOffset);
  }

  rx::die("unexpected memory mapped register address %x, %s", dwAddress,
          gnm::mmio::registerName(dwAddress));
}

bool GraphicsPipe::processAllRings() {
  bool allProcessed = true;

  if (ceQueue.rptr != ceQueue.wptr) {
    processRing(ceQueue);

    if (ceQueue.rptr != ceQueue.wptr) {
      allProcessed = false;
    }
  }

  for (auto &ring : deQueues) {
    if (ring.rptr == ring.wptr) {
      continue;
    }

    processRing(ring);

    if (ring.rptr != ring.wptr) {
      allProcessed = false;
      break;
    }
  }

  return allProcessed;
}

void GraphicsPipe::processRing(Ring &ring) {
  int cp;
  if (ring.indirectLevel < 0) {
    cp = 0;
  } else {
    cp = ring.indirectLevel + 1;
  }

  while (ring.rptr != ring.wptr) {
    if (ring.rptr >= ring.base + ring.size) {
      ring.rptr = ring.base;
      continue;
    }

    auto header = *ring.rptr;
    auto type = rx::getBits(header, 31, 30);

    if (type == 3) {
      auto op = rx::getBits(header, 15, 8);
      auto len = rx::getBits(header, 29, 16) + 2;

      // if (auto str = gnm::pm4OpcodeToString(op)) {
      //   std::println(stderr, "queue {}: {}", ring.indirectLevel, str);
      // } else {
      //   std::println(stderr, "queue {}: {:x}", ring.indirectLevel, op);
      // }

      if (op == gnm::IT_COND_EXEC) {
        rx::die("unimplemented COND_EXEC");
      }

      auto handler = commandHandlers[cp][op];
      if (!(this->*handler)(ring)) {
        return;
      }

      ring.rptr += len;

      if (op == gnm::IT_INDIRECT_BUFFER || op == gnm::IT_INDIRECT_BUFFER_CNST) {
        break;
      }

      continue;
    }

    if (type == 2) {
      ++ring.rptr;
      continue;
    }

    rx::die("unexpected pm4 packet type %u, ring %u, header %u, rptr %p, wptr "
            "%p, base %p",
            type, ring.indirectLevel, header, ring.rptr, ring.wptr, ring.base);
  }
}

bool GraphicsPipe::handleNop(Ring &ring) { return true; }

bool GraphicsPipe::setBase(Ring &ring) {
  auto baseIndex = ring.rptr[1] & 0xf;

  switch (baseIndex) {
  case 0: {
    auto address0 = ring.rptr[2] & ~3;
    auto address1 = ring.rptr[3] & ((1 << 16) - 1);

    displayListPatchBase =
        address0 | (static_cast<std::uint64_t>(address1) << 32);
    break;
  }
  case 1: {
    auto address0 = ring.rptr[2] & ~3;
    auto address1 = ring.rptr[3] & ((1 << 16) - 1);

    drawIndexIndirPatchBase =
        address0 | (static_cast<std::uint64_t>(address1) << 32);
    break;
  }

  case 2: {
    auto cs1Index = ring.rptr[2] & ((1 << 16) - 1);
    auto cs2Index = ring.rptr[3] & ((1 << 16) - 1);
    gdsPartitionBases[0] = cs1Index;
    gdsPartitionBases[1] = cs2Index;
    break;
  }

  case 3: {
    auto cs1Index = ring.rptr[2] & ((1 << 16) - 1);
    auto cs2Index = ring.rptr[3] & ((1 << 16) - 1);
    cePartitionBases[0] = cs1Index;
    cePartitionBases[1] = cs2Index;
    break;
  }

  default:
    rx::die("pm4: unknown SET_BASE index %u", baseIndex);
  }

  return true;
}

bool GraphicsPipe::clearState(Ring &ring) {
  auto paScClipRectRule = context.paScClipRectRule.value;
  auto cbTargetMask = context.cbTargetMask.raw;
  auto cbShaderMask = context.cbShaderMask.raw;
  auto vgtMaxVtxIndx = context.vgtMaxVtxIndx.value;
  auto vgtMinVtxIndx = context.vgtMinVtxIndx.value;
  auto vgtIndxOffset = context.vgtIndxOffset.value;
  auto paScAaMaskX0Y0_X1Y0 = context.paScAaMaskX0Y0_X1Y0.value;
  auto paScAaMaskX0Y1_X1Y1 = context.paScAaMaskX0Y1_X1Y1.value;

  context = Registers::Context::Default;

  context.paScClipRectRule.value = paScClipRectRule;
  context.cbTargetMask.raw = cbTargetMask;
  context.cbShaderMask.raw = cbShaderMask;
  context.vgtMaxVtxIndx.value = vgtMaxVtxIndx;
  context.vgtMinVtxIndx.value = vgtMinVtxIndx;
  context.vgtIndxOffset.value = vgtIndxOffset;
  context.paScAaMaskX0Y0_X1Y0.value = paScAaMaskX0Y0_X1Y0;
  context.paScAaMaskX0Y1_X1Y1.value = paScAaMaskX0Y1_X1Y1;
  return true;
}

bool GraphicsPipe::contextControl(Ring &ring) { return true; }
bool GraphicsPipe::acquireMem(Ring &ring) { return true; }
bool GraphicsPipe::releaseMem(Ring &ring) {
  auto eventCntl = ring.rptr[1];
  auto dataCntl = ring.rptr[2];
  auto addressLo = ring.rptr[3] & ~3;
  auto addressHi = ring.rptr[4] & ((1 << 16) - 1);
  auto dataLo = ring.rptr[5];
  auto dataHi = ring.rptr[6];

  auto eventIndex = rx::getBits(eventCntl, 11, 8);
  auto eventType = rx::getBits(eventCntl, 5, 0);
  auto dataSel = rx::getBits(dataCntl, 31, 29);
  auto intSel = rx::getBits(dataCntl, 25, 24);

  auto address = addressLo | (static_cast<std::uint64_t>(addressHi) << 32);
  auto pointer = RemoteMemory{ring.vmId}.getPointer<std::uint64_t>(address);

  context.vgtEventInitiator = eventType;

  switch (dataSel) {
  case 0: // none
    break;
  case 1: // 32 bit, low
    *reinterpret_cast<std::uint32_t *>(pointer) = dataLo;
    break;
  case 2: // 64 bit
    *pointer = dataLo | (static_cast<std::uint64_t>(dataHi) << 32);
    break;
  case 3: // 64 bit, global GPU clock
    *pointer = std::chrono::duration_cast<std::chrono::nanoseconds>(
                   std::chrono::system_clock::now().time_since_epoch())
                   .count();
    break;
  case 4: // 64 bit, perf counter
    *pointer = std::chrono::duration_cast<std::chrono::nanoseconds>(
                   std::chrono::steady_clock::now().time_since_epoch())
                   .count();
    break;

  default:
    rx::die("unimplemented event release mem data %#x", dataSel);
  }

  return true;
}

bool GraphicsPipe::drawPreamble(Ring &ring) { return true; }

bool GraphicsPipe::indexBufferSize(Ring &ring) {
  vgtIndexBufferSize = ring.rptr[1];
  return true;
}
bool GraphicsPipe::dispatchDirect(Ring &ring) {
  auto dimX = ring.rptr[1];
  auto dimY = ring.rptr[2];
  auto dimZ = ring.rptr[3];
  auto dispatchInitiator = ring.rptr[4];
  sh.compute.computeDispatchInitiator = dispatchInitiator;

  amdgpu::dispatch(device->caches[ring.vmId], scheduler, sh.compute, dimX, dimY,
                   dimZ);
  return true;
}
bool GraphicsPipe::dispatchIndirect(Ring &ring) {
  auto offset = ring.rptr[1];
  auto dispatchInitiator = ring.rptr[2];

  sh.compute.computeDispatchInitiator = dispatchInitiator;
  auto buffer = RemoteMemory{ring.vmId}.getPointer<std::uint32_t>(
      drawIndexIndirPatchBase + offset);

  auto dimX = buffer[0];
  auto dimY = buffer[1];
  auto dimZ = buffer[2];

  amdgpu::dispatch(device->caches[ring.vmId], scheduler, sh.compute, dimX, dimY,
                   dimZ);
  return true;
}

bool GraphicsPipe::setPredication(Ring &ring) {
  auto startAddressLo = ring.rptr[1] & ~0xf;
  auto predProperties = ring.rptr[2];

  auto startAddressHi = rx::getBits(predProperties, 15, 0);
  auto predBool = rx::getBit(predProperties, 8);
  auto hint = rx::getBit(predProperties, 12);
  auto predOp = rx::getBits(predProperties, 18, 16);
  auto cont = rx::getBit(predProperties, 31);

  switch (predOp) {
  case 0: // clear predicate
  case 1: // set ZPass predicate
  case 2: // set PrimCount predicate
    break;
  }

  // TODO

  return true;
}
bool GraphicsPipe::drawIndirect(Ring &ring) {
  auto dataOffset = ring.rptr[1];
  auto baseVtxLoc = ring.rptr[2] & ((1 << 16) - 1);
  auto startInstLoc = ring.rptr[3] & ((1 << 16) - 1);
  auto drawInitiator = ring.rptr[4];

  context.vgtDrawInitiator = drawInitiator;

  auto buffer = RemoteMemory{ring.vmId}.getPointer<std::uint32_t>(
      drawIndexIndirPatchBase + dataOffset);

  std::uint32_t vertexCountPerInstance = buffer[0];
  std::uint32_t instanceCount = buffer[1];
  std::uint32_t startVertexLocation = buffer[2];
  std::uint32_t startInstanceLocation = buffer[3];

  draw(*this, ring.vmId, startVertexLocation, vertexCountPerInstance,
       startInstanceLocation, instanceCount, 0, 0, 0);
  return true;
}
bool GraphicsPipe::drawIndexIndirect(Ring &ring) {
  auto dataOffset = ring.rptr[1];
  auto baseVtxLoc = ring.rptr[2] & ((1 << 16) - 1);
  auto drawInitiator = ring.rptr[3];

  auto buffer = RemoteMemory{ring.vmId}.getPointer<std::uint32_t>(
      drawIndexIndirPatchBase + dataOffset);

  context.vgtDrawInitiator = drawInitiator;

  std::uint32_t indexCountPerInstance = buffer[0];
  std::uint32_t instanceCount = buffer[1];
  std::uint32_t startIndexLocation = buffer[2];
  std::uint32_t baseVertexLocation = buffer[3];
  std::uint32_t startInstanceLocation = buffer[4];

  draw(*this, ring.vmId, baseVertexLocation, indexCountPerInstance,
       startInstanceLocation, instanceCount, vgtIndexBase, startIndexLocation,
       indexCountPerInstance);
  return true;
}
bool GraphicsPipe::indexBase(Ring &ring) {
  auto addressLo = ring.rptr[1] & ~1;
  auto addressHi = ring.rptr[2] & ((1 << 16) - 1);
  auto address = addressLo | (static_cast<std::uint64_t>(addressHi) << 32);
  vgtIndexBase = address;
  return true;
}
bool GraphicsPipe::drawIndex2(Ring &ring) {
  auto maxSize = ring.rptr[1];
  auto indexBaseLo = ring.rptr[2] & ~1;
  auto indexBaseHi = ring.rptr[3] & ((1 << 16) - 1);
  auto indexCount = ring.rptr[4];
  auto drawInitiator = ring.rptr[5];

  context.vgtDrawInitiator = drawInitiator;
  uConfig.vgtNumIndices = indexCount;

  auto indexBase =
      indexBaseLo | (static_cast<std::uint64_t>(indexBaseHi) << 32);

  draw(*this, ring.vmId, 0, indexCount, 0, uConfig.vgtNumInstances, indexBase,
       0, maxSize);
  return true;
}
bool GraphicsPipe::indexType(Ring &ring) {
  uConfig.vgtIndexType = static_cast<gnm::IndexType>(ring.rptr[1] & 1);
  return true;
}
bool GraphicsPipe::drawIndexAuto(Ring &ring) {
  auto indexCount = ring.rptr[1];
  auto drawInitiator = ring.rptr[2];

  uConfig.vgtNumIndices = indexCount;
  context.vgtDrawInitiator = drawInitiator;

  draw(*this, ring.vmId, 0, indexCount, 0, uConfig.vgtNumInstances, 0, 0, 0);
  return true;
}
bool GraphicsPipe::numInstances(Ring &ring) {
  uConfig.vgtNumInstances = std::max(ring.rptr[1], 1u);
  return true;
}
bool GraphicsPipe::drawIndexMultiAuto(Ring &ring) {
  auto primCount = ring.rptr[1];
  auto drawInitiator = ring.rptr[2];
  auto control = ring.rptr[3];

  auto indexOffset = rx::getBits(control, 15, 0);
  auto primType = rx::getBits(control, 20, 16);
  auto indexCount = rx::getBits(control, 31, 21);

  context.vgtDrawInitiator = drawInitiator;
  uConfig.vgtPrimitiveType = static_cast<gnm::PrimitiveType>(primType);
  uConfig.vgtNumIndices = indexCount;

  draw(*this, ring.vmId, 0, primCount, 0, uConfig.vgtNumInstances, vgtIndexBase,
       indexOffset, indexCount);
  return true;
}
bool GraphicsPipe::drawIndexOffset2(Ring &ring) {
  auto maxSize = ring.rptr[1];
  auto indexOffset = ring.rptr[2];
  auto indexCount = ring.rptr[3];
  auto drawInitiator = ring.rptr[4];

  context.vgtDrawInitiator = drawInitiator;
  draw(*this, ring.vmId, 0, indexCount, 0, uConfig.vgtNumInstances,
       vgtIndexBase, indexOffset, maxSize);
  return true;
}
bool GraphicsPipe::writeData(Ring &ring) {
  auto len = rx::getBits(ring.rptr[0], 29, 16) - 1;
  auto control = ring.rptr[1];
  auto dstAddressLo = ring.rptr[2];
  auto dstAddressHi = ring.rptr[3];
  auto data = ring.rptr + 4;

  auto engineSel = rx::getBits(control, 31, 30);
  auto wrConfirm = rx::getBit(control, 20);
  auto wrOneAddress = rx::getBit(control, 16);
  auto dstSel = rx::getBits(control, 11, 8);

  std::uint32_t *dstPointer = nullptr;

  switch (dstSel) {
  case 0: // memory mapped register
    dstPointer = getMmRegister(dstAddressLo & ((1 << 16) - 1));
    break;

  case 1:   // memory sync
  case 2:   // TC L2
  case 5: { // memory async
    auto address =
        (dstAddressLo & ~3) | (static_cast<std::uint64_t>(dstAddressHi) << 32);
    dstPointer = RemoteMemory{ring.vmId}.getPointer<std::uint32_t>(address);
    break;
  }

  default:
    rx::die("unimplemented write data, dst sel = %#x", dstSel);
  }

  if (wrOneAddress) {
    for (std::uint32_t i = 0; i < len; ++i) {
      *dstPointer = data[i];
    }
  } else {
    std::memcpy(dstPointer, data, len * sizeof(std::uint32_t));
  }

  return true;
}
bool GraphicsPipe::memSemaphore(Ring &ring) {
  // FIXME
  return true;
}
bool GraphicsPipe::waitRegMem(Ring &ring) {
  auto engine = rx::getBit(ring.rptr[1], 8);
  auto memSpace = rx::getBit(ring.rptr[1], 4);
  auto function = rx::getBits(ring.rptr[1], 2, 0);
  auto pollAddressLo = ring.rptr[2];
  auto pollAddressHi = ring.rptr[3] & ((1 << 16) - 1);
  auto reference = ring.rptr[4];
  auto mask = ring.rptr[5];
  auto pollInterval = ring.rptr[6];

  std::uint32_t pollData;

  if (memSpace == 0) {
    pollData = *getMmRegister(pollAddressLo & ((1 << 16) - 1));
  } else {
    auto pollAddress = (pollAddressLo & ~3) |
                       (static_cast<std::uint64_t>(pollAddressHi) << 32);
    pollData = *RemoteMemory{ring.vmId}.getPointer<std::uint32_t>(pollAddress);
  }

  return compare(function, pollData, mask, reference);
}

bool GraphicsPipe::indirectBufferConst(Ring &ring) {
  rx::dieIf(ring.indirectLevel < 0, "unexpected indirect buffer from CP");

  auto addressLo = ring.rptr[1] & ~3;
  auto addressHi = ring.rptr[2] & ((1 << 8) - 1);
  int vmId = ring.rptr[3] >> 24;
  auto ibSize = ring.rptr[3] & ((1 << 20) - 1);
  auto address = addressLo | (static_cast<std::uint64_t>(addressHi) << 32);

  if (ring.indirectLevel != 0) {
    vmId = ring.vmId;
  }

  auto rptr = RemoteMemory{vmId}.getPointer<std::uint32_t>(address);
  setCeQueue(Ring::createFromRange(vmId, rptr, ibSize));
  return true;
}
bool GraphicsPipe::indirectBuffer(Ring &ring) {
  rx::dieIf(ring.indirectLevel < 0, "unexpected indirect buffer from CP");

  auto addressLo = ring.rptr[1] & ~3;
  auto addressHi = ring.rptr[2] & ((1 << 8) - 1);
  int vmId = ring.rptr[3] >> 24;
  auto ibSize = ring.rptr[3] & ((1 << 20) - 1);
  auto address = addressLo | (static_cast<std::uint64_t>(addressHi) << 32);

  if (ring.indirectLevel != 0) {
    vmId = ring.vmId;
  }
  auto rptr = RemoteMemory{vmId}.getPointer<std::uint32_t>(address);
  setDeQueue(Ring::createFromRange(vmId, rptr, ibSize), ring.indirectLevel + 1);
  return true;
}
bool GraphicsPipe::pfpSyncMe(Ring &ring) {
  // TODO
  return true;
}
bool GraphicsPipe::condWrite(Ring &ring) {
  auto writeSpace = rx::getBit(ring.rptr[1], 8);
  auto pollSpace = rx::getBit(ring.rptr[1], 4);
  auto function = rx::getBits(ring.rptr[1], 2, 0);
  auto pollAddressLo = ring.rptr[2];
  auto pollAddressHi = ring.rptr[3] & ((1 << 16) - 1);
  auto reference = ring.rptr[4];
  auto mask = ring.rptr[5];
  auto writeAddressLo = ring.rptr[6];
  auto writeAddressHi = ring.rptr[7] & ((1 << 16) - 1);
  auto writeData = ring.rptr[8];

  std::uint32_t pollData;

  if (pollSpace == 0) {
    pollData = *getMmRegister(pollAddressLo & ((1 << 16) - 1));
  } else {
    auto pollAddress = (pollAddressLo & ~3) |
                       (static_cast<std::uint64_t>(pollAddressHi) << 32);
    pollData = *RemoteMemory{ring.vmId}.getPointer<std::uint32_t>(pollAddress);
  }

  if (compare(function, pollData, mask, reference)) {
    if (writeSpace == 0) {
      *getMmRegister(writeAddressLo & ((1 << 16) - 1)) = writeData;
    } else {
      auto writeAddress = (writeAddressLo & ~3) |
                          (static_cast<std::uint64_t>(writeAddressHi) << 32);

      *RemoteMemory{ring.vmId}.getPointer<std::uint32_t>(writeAddress) =
          writeData;
    }
  }

  return true;
}

bool GraphicsPipe::eventWrite(Ring &ring) {
  enum {
    kEventZPassDone = 1,
    kEventSamplePipelineStat = 2,
    kEventSampleStreamOutStat = 3,
    kEventPartialFlush = 4,
  };

  auto eventCntl = ring.rptr[1];
  auto invL2 = rx::getBit(eventCntl, 20);
  auto eventIndex = rx::getBits(eventCntl, 11, 8);
  auto eventType = rx::getBits(eventCntl, 5, 0);

  context.vgtEventInitiator = eventType;

  if (eventIndex == kEventZPassDone || eventIndex == kEventSamplePipelineStat ||
      eventIndex == kEventSampleStreamOutStat) {
    auto addressLo = ring.rptr[2] & ~7;
    auto addressHi = ring.rptr[3] & ((1 << 16) - 1);
    auto address = addressLo | (static_cast<std::uint64_t>(addressHi) << 32);
    rx::die("unimplemented event write, event index %#x, address %lx",
            eventIndex, address);
    return true;
  }

  // FIXME
  return true;
}

bool GraphicsPipe::eventWriteEop(Ring &ring) {
  auto eventCntl = ring.rptr[1];
  auto addressLo = ring.rptr[2] & ~3;
  auto dataCntl = ring.rptr[3];
  auto dataLo = ring.rptr[4];
  auto dataHi = ring.rptr[5];

  auto invL2 = rx::getBit(eventCntl, 20);
  auto eventIndex = rx::getBits(eventCntl, 11, 8);
  auto eventType = rx::getBits(eventCntl, 5, 0);
  auto dataSel = rx::getBits(dataCntl, 31, 29);
  auto intSel = rx::getBits(dataCntl, 25, 24);
  auto addressHi = rx::getBits(dataCntl, 15, 0);

  auto address = addressLo | (static_cast<std::uint64_t>(addressHi) << 32);
  auto pointer = RemoteMemory{ring.vmId}.getPointer<std::uint64_t>(address);

  context.vgtEventInitiator = eventType;

  switch (dataSel) {
  case 0: // none
    break;
  case 1: // 32 bit, low
    *reinterpret_cast<std::uint32_t *>(pointer) = dataLo;
    break;
  case 2: // 64 bit
    *pointer = dataLo | (static_cast<std::uint64_t>(dataHi) << 32);
    break;
  case 3: // 64 bit, global GPU clock
    *pointer = std::chrono::duration_cast<std::chrono::nanoseconds>(
                   std::chrono::system_clock::now().time_since_epoch())
                   .count();
    break;
  case 4: // 64 bit, perf counter
    *pointer = std::chrono::duration_cast<std::chrono::nanoseconds>(
                   std::chrono::steady_clock::now().time_since_epoch())
                   .count();
    break;

  default:
    rx::die("unimplemented event write eop data %#x", dataSel);
  }

  if (intSel) {
    orbis::g_context.deviceEventEmitter->emit(orbis::kEvFiltGraphicsCore, 0,
                                              kGcEventGfxEop);
  }

  return true;
}

bool GraphicsPipe::eventWriteEos(Ring &ring) {
  auto eventCntl = ring.rptr[1];
  auto addressLo = ring.rptr[2] & ~3;
  auto cmdInfo = ring.rptr[3];
  auto dataInfo = ring.rptr[4];

  auto eventIndex = rx::getBits(eventCntl, 11, 8);
  auto eventType = rx::getBits(eventCntl, 5, 0);
  auto cmd = rx::getBits(cmdInfo, 31, 29);
  auto addressHi = rx::getBits(cmdInfo, 15, 0);

  auto address = addressLo | (static_cast<std::uint64_t>(addressHi) << 32);
  auto pointer = RemoteMemory{ring.vmId}.getPointer<std::uint32_t>(address);

  context.vgtEventInitiator = eventType;
  auto &cache = device->caches[ring.vmId];

  switch (cmd) {
  case 1: { // store GDS data to memory
    auto sizeDw = rx::getBits(dataInfo, 31, 16);
    auto gdsIndexDw = rx::getBits(dataInfo, 15, 0);
    std::println("event write eos: gds data {:x}-{:x}", gdsIndexDw,
                 gdsIndexDw + sizeDw);
    auto size = sizeof(std::uint32_t) * sizeDw;

    auto gds = cache.getGdsBuffer().getData();
    cache.invalidate(scheduler, rx::AddressRange::fromBeginSize(address, size));
    std::memcpy(pointer, gds + gdsIndexDw * sizeof(std::uint32_t), size);
    break;
  }

  case 2: // after GDS writes confirm, store 32 bit DATA to memory as fence
    cache.invalidate(scheduler, rx::AddressRange::fromBeginSize(
                                    address, sizeof(std::uint32_t)));
    *pointer = dataInfo;
    break;

  default:
    rx::die("unexpected event write eos command: %#x", cmd);
  }
  return true;
}

bool GraphicsPipe::dmaData(Ring &ring) {
  auto control = ring.rptr[1];
  auto srcAddressLo = ring.rptr[2];
  auto data = srcAddressLo;
  auto srcAddressHi = ring.rptr[3];
  auto dstAddressLo = ring.rptr[4];
  auto dstAddressHi = ring.rptr[5];
  auto cmdSize = ring.rptr[6];
  auto size = rx::getBits(cmdSize, 20, 0);

  auto engine = rx::getBit(control, 0);
  auto srcVolatile = rx::getBit(control, 15);

  // 0 - dstAddress using das
  // 1 - gds
  // 3 - dstAddress using L2
  auto dstSel = rx::getBits(control, 21, 20);

  // 0 - LRU
  // 1 - Stream
  // 2 - Bypass
  auto dstCachePolicy = rx::getBits(control, 26, 25);

  auto dstVolatile = rx::getBit(control, 27);

  // 0 - srcAddress using sas
  // 1 - gds
  // 2 - data
  // 3 - srcAddress using L2
  auto srcSel = rx::getBits(control, 30, 29);

  auto cpSync = rx::getBit(control, 31);

  auto dataDisWc = rx::getBit(cmdSize, 21);

  // 0 - none
  // 1 - 8 in 16
  // 2 - 8 in 32
  // 3 - 8 in 64
  auto dstSwap = rx::getBits(cmdSize, 25, 24);

  // 0 - memory
  // 1 - register
  auto sas = rx::getBit(cmdSize, 26);

  // 0 - memory
  // 1 - register
  auto das = rx::getBit(cmdSize, 27);

  auto saic = rx::getBit(cmdSize, 28);
  auto daic = rx::getBit(cmdSize, 29);
  auto rawWait = rx::getBit(cmdSize, 30);

  void *dst = nullptr;
  switch (dstSel) {
  case 3:
  case 0:
    if (dstSel == 3 || das == 0) {
      auto dstAddress =
          dstAddressLo | (static_cast<std::uint64_t>(dstAddressHi) << 32);
      dst = amdgpu::RemoteMemory{ring.vmId}.getPointer(dstAddress);
      device->caches[ring.vmId].invalidate(
          scheduler, rx::AddressRange::fromBeginSize(dstAddress, size));
    } else {
      dst = getMmRegister(dstAddressLo / sizeof(std::uint32_t));
    }
    break;

  case 1:
    dst = device->caches[ring.vmId].getGdsBuffer().getData() + dstAddressLo;
    break;

  default:
    rx::die("IT_DMA_DATA: unexpected dstSel %u", dstSel);
  }

  void *src = nullptr;
  std::uint32_t srcSize = 0;
  switch (srcSel) {
  case 3:
  case 0:
    if (srcSel == 3 || sas == 0) {
      auto srcAddress =
          srcAddressLo | (static_cast<std::uint64_t>(srcAddressHi) << 32);
      src = amdgpu::RemoteMemory{ring.vmId}.getPointer(srcAddress);
      device->caches[ring.vmId].flush(
          scheduler, rx::AddressRange::fromBeginSize(srcAddress, size));
    } else {
      src = getMmRegister(srcAddressLo / sizeof(std::uint32_t));
    }

    srcSize = ~0;
    break;
  case 1:
    src = device->caches[ring.vmId].getGdsBuffer().getData() + srcAddressLo;
    srcSize = ~0;
    break;

  case 2:
    src = &data;
    srcSize = sizeof(data);
    saic = 1;
    break;

  default:
    rx::die("IT_DMA_DATA: unexpected srcSel %u", srcSel);
  }

  rx::dieIf(size > srcSize && saic == 0,
            "IT_DMA_DATA: out of source size srcSel %u, dstSel %u, size %u",
            srcSel, dstSel, size);

  if (saic != 0) {
    if (daic != 0 && dstSel == 0 && das == 1) {
      std::memcpy(dst, src, sizeof(std::uint32_t));
    } else {
      for (std::uint32_t i = 0; i < size / sizeof(std::uint32_t); ++i) {
        std::memcpy(std::bit_cast<std::uint32_t *>(dst) + i, src,
                    sizeof(std::uint32_t));
      }
    }
  } else if (daic != 0 && dstSel == 0 && das == 1) {
    for (std::uint32_t i = 0; i < size / sizeof(std::uint32_t); ++i) {
      std::memcpy(dst, std::bit_cast<std::uint32_t *>(src) + i,
                  sizeof(std::uint32_t));
    }
  } else {
    std::memcpy(dst, src, size);
  }
  return true;
}

bool GraphicsPipe::setConfigReg(Ring &ring) {
  auto len = rx::getBits(ring.rptr[0], 29, 16);
  auto offset = ring.rptr[1] & 0xffff;
  auto data = ring.rptr + 2;

  rx::dieIf(
      (offset + len) * sizeof(std::uint32_t) > sizeof(device->config),
      "out of Config regs, offset: %x, count %u, %s\n", offset, len,
      gnm::mmio::registerName(decltype(device->config)::kMmioOffset + offset));

  std::memcpy(reinterpret_cast<std::uint32_t *>(&device->config) + offset, data,
              sizeof(std::uint32_t) * len);

  return true;
}

bool GraphicsPipe::setShReg(Ring &ring) {
  auto len = rx::getBits(ring.rptr[0], 29, 16);
  auto offset = ring.rptr[1] & 0xffff;
  auto index = ring.rptr[1] >> 26;
  auto data = ring.rptr + 2;

  rx::dieIf((offset + len) * sizeof(std::uint32_t) > sizeof(sh),
            "out of SH regs, offset: %x, count %u, %s\n", offset, len,
            gnm::mmio::registerName(decltype(sh)::kMmioOffset + offset));

  std::memcpy(reinterpret_cast<std::uint32_t *>(&sh) + offset, data,
              sizeof(std::uint32_t) * len);
  // for (std::size_t i = 0; i < len; ++i) {
  //   std::fprintf(
  //       stderr, "writing to %s value %x\n",
  //       gnm::mmio::registerName(decltype(sh)::kMmioOffset + offset + i),
  //       data[i]);
  // }
  return true;
}

bool GraphicsPipe::setUConfigReg(Ring &ring) {
  auto len = rx::getBits(ring.rptr[0], 29, 16);
  auto offset = ring.rptr[1] & 0xffff;
  auto index = ring.rptr[1] >> 26;
  auto data = ring.rptr + 2;

  if (index != 0) {
    {
      auto name =
          gnm::mmio::registerName(decltype(uConfig)::kMmioOffset + offset);
      std::println(
          stderr,
          "set UConfig regs with index, offset: {:x}, count {}, index {}, {}",
          offset, len, index, name ? name : "<null>");
    }

    for (std::size_t i = 0; i < len; ++i) {
      auto id = decltype(uConfig)::kMmioOffset + offset + i;
      if (auto regName = gnm::mmio::registerName(id)) {
        std::println(stderr, "writing to {} value {:x}", regName, data[i]);
      } else {
        std::println(stderr, "writing to {:x} value {:x}", id, data[i]);
      }
    }
  }

  rx::dieIf((offset + len) * sizeof(std::uint32_t) > sizeof(context),
            "out of UConfig regs, offset: %u, count %u, %s\n", offset, len,
            gnm::mmio::registerName(decltype(uConfig)::kMmioOffset + offset));

  std::memcpy(reinterpret_cast<std::uint32_t *>(&uConfig) + offset, data,
              sizeof(std::uint32_t) * len);
  // for (std::size_t i = 0; i < len; ++i) {
  //   std::fprintf(
  //       stderr, "writing to %s value %x\n",
  //       gnm::mmio::registerName(decltype(uConfig)::kMmioOffset + offset + i),
  //       data[i]);
  // }
  return true;
}

bool GraphicsPipe::setContextReg(Ring &ring) {
  auto len = rx::getBits(ring.rptr[0], 29, 16);
  auto offset = ring.rptr[1] & 0xffff;
  auto index = ring.rptr[1] >> 26;
  auto data = ring.rptr + 2;

  if (index != 0) {
    {
      auto name =
          gnm::mmio::registerName(decltype(context)::kMmioOffset + offset);
      std::println(
          stderr,
          "set Context regs with index, offset: {:x}, count {}, index {}, {}",
          offset, len, index, name ? name : "<null>");
    }

    for (std::size_t i = 0; i < len; ++i) {
      auto id = decltype(context)::kMmioOffset + offset + i;
      if (auto regName = gnm::mmio::registerName(id)) {
        std::println(stderr, "writing to {} value {:x}", regName, data[i]);
      } else {
        std::println(stderr, "writing to {:x} value {:x}", id, data[i]);
      }
    }
  }

  rx::dieIf((offset + len) * sizeof(std::uint32_t) > sizeof(context),
            "out of Context regs, offset: %u, count %u, %s\n", offset, len,
            gnm::mmio::registerName(decltype(context)::kMmioOffset + offset));

  std::memcpy(reinterpret_cast<std::uint32_t *>(&context) + offset, data,
              sizeof(std::uint32_t) * len);

  // for (std::size_t i = 0; i < len; ++i) {
  //   std::fprintf(
  //       stderr, "writing to %s value %x\n",
  //       gnm::mmio::registerName(decltype(context)::kMmioOffset + offset + i),
  //       data[i]);
  // }
  return true;
}

bool GraphicsPipe::setCeDeCounters(Ring &ring) {
  auto counterLo = ring.rptr[1];
  auto counterHi = ring.rptr[2];
  auto counter = counterLo | (static_cast<std::uint64_t>(counterHi) << 32);
  deCounter = counter;
  ceCounter = counter;
  return true;
}

bool GraphicsPipe::waitOnCeCounter(Ring &ring) {
  auto counterLo = ring.rptr[1];
  auto counterHi = ring.rptr[2];
  auto counter = counterLo | (static_cast<std::uint64_t>(counterHi) << 32);
  return deCounter >= counter;
}

bool GraphicsPipe::waitOnDeCounterDiff(Ring &ring) {
  auto waitDiff = ring.rptr[1];
  auto diff = ceCounter - deCounter;
  return diff < waitDiff;
}

bool GraphicsPipe::incrementCeCounter(Ring &) {
  ceCounter++;
  return true;
}

bool GraphicsPipe::incrementDeCounter(Ring &) {
  deCounter++;
  return true;
}

bool GraphicsPipe::loadConstRam(Ring &ring) {
  std::uint32_t addressLo = ring.rptr[1];
  std::uint32_t addressHi = ring.rptr[2];
  std::uint32_t numDw = ring.rptr[3] & ((1 << 15) - 1);
  std::uint32_t offset =
      (ring.rptr[4] & ((1 << 16) - 1)) / sizeof(std::uint32_t);
  auto address = addressLo | (static_cast<std::uint64_t>(addressHi) << 32);
  std::memcpy(constantMemory + offset,
              RemoteMemory{ring.vmId}.getPointer(address),
              numDw * sizeof(std::uint32_t));

  return true;
}

bool GraphicsPipe::writeConstRam(Ring &ring) {
  std::uint32_t offset =
      (ring.rptr[1] & ((1 << 16) - 1)) / sizeof(std::uint32_t);
  std::uint32_t data = ring.rptr[2];
  std::memcpy(constantMemory + offset, &data, sizeof(std::uint32_t));
  return true;
}

bool GraphicsPipe::dumpConstRam(Ring &ring) {
  std::uint32_t offset =
      (ring.rptr[1] & ((1 << 16) - 1)) / sizeof(std::uint32_t);
  std::uint32_t numDw = ring.rptr[2] & ((1 << 15) - 1);
  std::uint32_t addressLo = ring.rptr[3];
  std::uint32_t addressHi = ring.rptr[4];
  auto address = addressLo | (static_cast<std::uint64_t>(addressHi) << 32);
  std::memcpy(RemoteMemory{ring.vmId}.getPointer(address),
              constantMemory + offset, numDw * sizeof(std::uint32_t));

  return true;
}

bool GraphicsPipe::unknownPacket(Ring &ring) {
  auto op = rx::getBits(ring.rptr[0], 15, 8);

  rx::die("unimplemented gfx pm4 packet: %s, queue %u\n",
          gnm::pm4OpcodeToString(op), ring.indirectLevel);
}

bool GraphicsPipe::switchBuffer(Ring &ring) {
  // FIXME: implement
  return true;
}

bool GraphicsPipe::mapProcess(Ring &ring) {
  auto pid = ring.rptr[1];
  int vmId = ring.rptr[2];

  device->mapProcess(pid, vmId);
  return true;
}

bool GraphicsPipe::mapQueues(Ring &ring) {
  // FIXME: implement
  return true;
}

bool GraphicsPipe::unmapQueues(Ring &ring) {
  // FIXME: implement
  return true;
}

bool GraphicsPipe::mapMemory(Ring &ring) {
  auto pid = ring.rptr[1];
  auto addressLo = ring.rptr[2];
  auto addressHi = ring.rptr[3];
  auto sizeLo = ring.rptr[4];
  auto sizeHi = ring.rptr[5];
  auto memoryType = ring.rptr[6];
  auto dmemIndex = ring.rptr[7];
  auto prot = ring.rptr[8];
  auto offsetLo = ring.rptr[9];
  auto offsetHi = ring.rptr[10];

  auto address = addressLo | (static_cast<std::uint64_t>(addressHi) << 32);
  auto size = sizeLo | (static_cast<std::uint64_t>(sizeHi) << 32);
  auto offset = offsetLo | (static_cast<std::uint64_t>(offsetHi) << 32);

  device->mapMemory(pid, address, size, memoryType, dmemIndex, prot, offset);
  return true;
}
bool GraphicsPipe::unmapMemory(Ring &ring) {
  auto pid = ring.rptr[1];
  auto addressLo = ring.rptr[2];
  auto addressHi = ring.rptr[3];
  auto sizeLo = ring.rptr[4];
  auto sizeHi = ring.rptr[5];

  auto address = addressLo | (static_cast<std::uint64_t>(addressHi) << 32);
  auto size = sizeLo | (static_cast<std::uint64_t>(sizeHi) << 32);
  device->unmapMemory(pid, address, size);
  return true;
}
bool GraphicsPipe::protectMemory(Ring &ring) {
  auto pid = ring.rptr[1];
  auto addressLo = ring.rptr[2];
  auto addressHi = ring.rptr[3];
  auto sizeLo = ring.rptr[4];
  auto sizeHi = ring.rptr[5];
  auto prot = ring.rptr[6];
  auto address = addressLo | (static_cast<std::uint64_t>(addressHi) << 32);
  auto size = sizeLo | (static_cast<std::uint64_t>(sizeHi) << 32);

  device->protectMemory(pid, address, size, prot);
  return true;
}
bool GraphicsPipe::unmapProcess(Ring &ring) {
  auto pid = ring.rptr[1];
  device->unmapProcess(pid);
  return true;
}

bool GraphicsPipe::flip(Ring &ring) {
  auto buffer = ring.rptr[1];
  auto dataLo = ring.rptr[2];
  auto dataHi = ring.rptr[3];
  auto pid = ring.rptr[4];
  auto data = dataLo | (static_cast<std::uint64_t>(dataHi) << 32);

  device->flip(pid, buffer, data);
  return true;
}
