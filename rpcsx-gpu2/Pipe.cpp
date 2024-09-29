#include "Pipe.hpp"
#include "Device.hpp"
#include "Registers.hpp"
#include "Renderer.hpp"
#include "gnm/mmio.hpp"
#include "gnm/pm4.hpp"
#include "vk.hpp"
#include <cstdio>
#include <rx/bits.hpp>
#include <rx/die.hpp>
#include <vulkan/vulkan_core.h>

using namespace amdgpu;

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

ComputePipe::ComputePipe(int index) : scheduler(createComputeScheduler(index)) {
  for (auto &handler : commandHandlers) {
    handler = &ComputePipe::unknownPacket;
  }

  commandHandlers[gnm::IT_NOP] = &ComputePipe::handleNop;
}

bool ComputePipe::processAllRings() {
  bool allProcessed = true;

  for (auto &ring : queues) {
    processRing(ring);

    if (ring.rptr != ring.wptr) {
      allProcessed = false;
      break;
    }
  }

  return allProcessed;
}

void ComputePipe::processRing(Queue &queue) {
  while (queue.rptr != queue.wptr) {
    if (queue.rptr >= queue.base + queue.size) {
      queue.rptr = queue.base;
    }

    auto header = *queue.rptr;
    auto type = rx::getBits(header, 31, 30);

    if (type == 3) {
      auto op = rx::getBits(header, 15, 8);
      auto len = rx::getBits(header, 29, 16) + 2;

      // std::fprintf(stderr, "queue %d: %s\n", queue.indirectLevel,
      //              gnm::pm4OpcodeToString(op));

      if (op == gnm::IT_COND_EXEC) {
        rx::die("unimplemented COND_EXEC");
      }

      auto handler = commandHandlers[op];
      if (!(this->*handler)(queue)) {
        return;
      }

      queue.rptr += len;
      continue;
    }

    if (type == 2) {
      ++queue.rptr;
      continue;
    }

    rx::die("unexpected pm4 packet type %u", type);
  }
}

bool ComputePipe::unknownPacket(Queue &queue) {
  auto op = rx::getBits(queue.rptr[0], 15, 8);

  rx::die("unimplemented compute pm4 packet: %s, queue %u\n",
          gnm::pm4OpcodeToString(op), queue.indirectLevel);

  return true;
}

bool ComputePipe::handleNop(Queue &queue) { return true; }

GraphicsPipe::GraphicsPipe(int index) : scheduler(createGfxScheduler(index)) {
  for (auto &processorHandlers : commandHandlers) {
    for (auto &handler : processorHandlers) {
      handler = &GraphicsPipe::unknownPacket;
    }

    processorHandlers[gnm::IT_NOP] = &GraphicsPipe::handleNop;
  }

  auto &dataHandlers = commandHandlers[2];
  auto &deHandlers = commandHandlers[1];
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

  // IT_INDIRECT_BUFFER_CNST
  // IT_STRMOUT_BUFFER_UPDATE

  deHandlers[gnm::IT_DRAW_INDEX_OFFSET_2] = &GraphicsPipe::drawIndexOffset2;
  deHandlers[gnm::IT_DRAW_PREAMBLE] = &GraphicsPipe::drawPreamble;

  deHandlers[gnm::IT_WRITE_DATA] = &GraphicsPipe::writeData;
  deHandlers[gnm::IT_MEM_SEMAPHORE] = &GraphicsPipe::memSemaphore;
  // IT_COPY_DW
  deHandlers[gnm::IT_WAIT_REG_MEM] = &GraphicsPipe::waitRegMem;
  deHandlers[gnm::IT_INDIRECT_BUFFER] = &GraphicsPipe::indirectBuffer;
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
  // IT_SWITCH_BUFFER
  // IT_SET_RESOURCES
  // IT_MAP_PROCESS
  // IT_MAP_QUEUES
  // IT_UNMAP_QUEUES
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
}

void GraphicsPipe::setCeQueue(Queue queue) {
  queue.indirectLevel = -1;
  ceQueue = queue;
}

void GraphicsPipe::setDeQueue(Queue queue, int ring) {
  rx::dieIf(ring > 2, "out of indirect gfx rings, %u", ring);
  queue.indirectLevel = ring;
  deQueues[2 - ring] = queue;
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

  for (int i = 0; i < 3; ++i) {
    auto &queue = deQueues[i];

    if (queue.rptr == queue.wptr) {
      continue;
    }

    processRing(queue);

    if (queue.rptr != queue.wptr) {
      allProcessed = false;
      break;
    }
  }

  return allProcessed;
}

void GraphicsPipe::processRing(Queue &queue) {
  auto cp = 1;
  if (queue.indirectLevel < 0) {
    cp = 0;
  } else if (queue.indirectLevel == 2) {
    cp = 2;
  }

  while (queue.rptr != queue.wptr) {
    if (queue.rptr >= queue.base + queue.size) {
      queue.rptr = queue.base;
    }

    auto header = *queue.rptr;
    auto type = rx::getBits(header, 31, 30);

    if (type == 3) {
      auto op = rx::getBits(header, 15, 8);
      auto len = rx::getBits(header, 29, 16) + 2;

      // std::fprintf(stderr, "queue %d: %s\n", queue.indirectLevel,
      //              gnm::pm4OpcodeToString(op));

      if (op == gnm::IT_COND_EXEC) {
        rx::die("unimplemented COND_EXEC");
      }

      auto handler = commandHandlers[cp][op];
      if (!(this->*handler)(queue)) {
        return;
      }

      queue.rptr += len;

      if (op == gnm::IT_INDIRECT_BUFFER || op == gnm::IT_INDIRECT_BUFFER_CNST) {
        break;
      }

      continue;
    }

    if (type == 2) {
      ++queue.rptr;
      continue;
    }

    rx::die("unexpected pm4 packet type %u", type);
  }
}

bool GraphicsPipe::handleNop(Queue &queue) { return true; }

bool GraphicsPipe::setBase(Queue &queue) {
  auto baseIndex = queue.rptr[1] & 0xf;

  switch (baseIndex) {
  case 0: {
    auto address0 = queue.rptr[2] & ~3;
    auto address1 = queue.rptr[3] & ((1 << 16) - 1);

    displayListPatchBase =
        address0 | (static_cast<std::uint64_t>(address1) << 32);
    break;
  }
  case 1: {
    auto address0 = queue.rptr[2] & ~3;
    auto address1 = queue.rptr[3] & ((1 << 16) - 1);

    drawIndexIndirPatchBase =
        address0 | (static_cast<std::uint64_t>(address1) << 32);
    break;
  }

  case 2: {
    auto cs1Index = queue.rptr[2] & ((1 << 16) - 1);
    auto cs2Index = queue.rptr[3] & ((1 << 16) - 1);
    gdsPartitionBases[0] = cs1Index;
    gdsPartitionBases[1] = cs2Index;
    break;
  }

  case 3: {
    auto cs1Index = queue.rptr[2] & ((1 << 16) - 1);
    auto cs2Index = queue.rptr[3] & ((1 << 16) - 1);
    cePartitionBases[0] = cs1Index;
    cePartitionBases[1] = cs2Index;
    break;
  }

  default:
    rx::die("pm4: unknown SET_BASE index %u", baseIndex);
  }

  return true;
}

bool GraphicsPipe::clearState(Queue &queue) {
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

bool GraphicsPipe::contextControl(Queue &queue) { return true; }
bool GraphicsPipe::acquireMem(Queue &queue) { return true; }
bool GraphicsPipe::releaseMem(Queue &queue) {
  auto eventCntl = queue.rptr[1];
  auto dataCntl = queue.rptr[2];
  auto addressLo = queue.rptr[3] & ~3;
  auto addressHi = queue.rptr[3] & ~3;
  auto dataLo = queue.rptr[4];
  auto dataHi = queue.rptr[5];

  auto eventIndex = rx::getBits(eventCntl, 11, 8);
  auto eventType = rx::getBits(eventCntl, 5, 0);
  auto dataSel = rx::getBits(dataCntl, 31, 29);
  auto intSel = rx::getBits(dataCntl, 25, 24);

  auto address = addressLo | (static_cast<std::uint64_t>(addressHi) << 32);
  auto pointer = RemoteMemory{queue.vmId}.getPointer<std::uint64_t>(address);

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

bool GraphicsPipe::drawPreamble(Queue &queue) { return true; }

bool GraphicsPipe::indexBufferSize(Queue &queue) {
  vgtIndexBufferSize = queue.rptr[1];
  return true;
}
bool GraphicsPipe::dispatchDirect(Queue &queue) {
  auto dimX = queue.rptr[1];
  auto dimY = queue.rptr[2];
  auto dimZ = queue.rptr[3];
  auto dispatchInitiator = queue.rptr[4];
  sh.compute.computeDispatchInitiator = dispatchInitiator;

  // FIXME
  return true;
}
bool GraphicsPipe::dispatchIndirect(Queue &queue) {
  auto offset = queue.rptr[1];
  auto dispatchInitiator = queue.rptr[2];

  sh.compute.computeDispatchInitiator = dispatchInitiator;
  auto buffer = RemoteMemory{queue.vmId}.getPointer<std::uint32_t>(
      drawIndexIndirPatchBase + offset);

  auto dimX = buffer[0];
  auto dimY = buffer[1];
  auto dimZ = buffer[2];

  // FIXME
  return true;
}

bool GraphicsPipe::setPredication(Queue &queue) {
  auto startAddressLo = queue.rptr[1] & ~0xf;
  auto predProperties = queue.rptr[2];

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
bool GraphicsPipe::drawIndirect(Queue &queue) {
  auto dataOffset = queue.rptr[1];
  auto baseVtxLoc = queue.rptr[2] & ((1 << 16) - 1);
  auto startInstLoc = queue.rptr[3] & ((1 << 16) - 1);
  auto drawInitiator = queue.rptr[4];

  context.vgtDrawInitiator = drawInitiator;

  auto buffer = RemoteMemory{queue.vmId}.getPointer<std::uint32_t>(
      drawIndexIndirPatchBase + dataOffset);

  std::uint32_t vertexCountPerInstance = buffer[0];
  std::uint32_t instanceCount = buffer[1];
  std::uint32_t startVertexLocation = buffer[2];
  std::uint32_t startInstanceLocation = buffer[3];

  draw(*this, queue.vmId, startVertexLocation, vertexCountPerInstance,
       startInstanceLocation, instanceCount, 0, 0);
  return true;
}
bool GraphicsPipe::drawIndexIndirect(Queue &queue) {
  auto dataOffset = queue.rptr[1];
  auto baseVtxLoc = queue.rptr[2] & ((1 << 16) - 1);
  auto drawInitiator = queue.rptr[3];

  auto buffer = RemoteMemory{queue.vmId}.getPointer<std::uint32_t>(
      drawIndexIndirPatchBase + dataOffset);

  context.vgtDrawInitiator = drawInitiator;

  std::uint32_t indexCountPerInstance = buffer[0];
  std::uint32_t instanceCount = buffer[1];
  std::uint32_t startIndexLocation = buffer[2];
  std::uint32_t baseVertexLocation = buffer[3];
  std::uint32_t startInstanceLocation = buffer[4];

  draw(*this, queue.vmId, baseVertexLocation, indexCountPerInstance,
       startInstanceLocation, instanceCount, vgtIndexBase + startIndexLocation,
       indexCountPerInstance);
  return true;
}
bool GraphicsPipe::indexBase(Queue &queue) {
  auto addressLo = queue.rptr[1] << 1;
  auto addressHi = queue.rptr[2] & ((1 << 16) - 1);
  auto address = addressLo | (static_cast<std::uint64_t>(addressHi) << 32);
  vgtIndexBase = address;
  return true;
}
bool GraphicsPipe::drawIndex2(Queue &queue) {
  auto maxSize = queue.rptr[1];
  auto indexOffset = queue.rptr[2];
  auto indexCount = queue.rptr[3];
  auto drawInitiator = queue.rptr[4];

  context.vgtDrawInitiator = drawInitiator;
  uConfig.vgtNumIndices = indexCount;

  draw(*this, queue.vmId, 0, indexCount, 0, uConfig.vgtNumInstances,
       vgtIndexBase + indexOffset, maxSize);
  return true;
}
bool GraphicsPipe::indexType(Queue &queue) {
  uConfig.vgtIndexType = static_cast<gnm::IndexType>(queue.rptr[1] & 1);
  return true;
}
bool GraphicsPipe::drawIndexAuto(Queue &queue) {
  auto indexCount = queue.rptr[1];
  auto drawInitiator = queue.rptr[2];

  uConfig.vgtNumIndices = indexCount;
  context.vgtDrawInitiator = drawInitiator;

  draw(*this, queue.vmId, 0, indexCount, 0, uConfig.vgtNumInstances, 0, 0);
  return true;
}
bool GraphicsPipe::numInstances(Queue &queue) {
  uConfig.vgtNumInstances = std::max(queue.rptr[1], 1u);
  return true;
}
bool GraphicsPipe::drawIndexMultiAuto(Queue &queue) {
  auto primCount = queue.rptr[1];
  auto drawInitiator = queue.rptr[2];
  auto control = queue.rptr[3];

  auto indexOffset = rx::getBits(control, 15, 0);
  auto primType = rx::getBits(control, 20, 16);
  auto indexCount = rx::getBits(control, 31, 21);

  context.vgtDrawInitiator = drawInitiator;
  uConfig.vgtPrimitiveType = static_cast<gnm::PrimitiveType>(primType);
  uConfig.vgtNumIndices = indexCount;

  draw(*this, queue.vmId, 0, indexCount, 0, uConfig.vgtNumInstances,
       vgtIndexBase + indexOffset, primCount);
  return true;
}
bool GraphicsPipe::drawIndexOffset2(Queue &queue) {
  auto maxSize = queue.rptr[1];
  auto indexOffset = queue.rptr[2];
  auto indexCount = queue.rptr[3];
  auto drawInitiator = queue.rptr[4];

  context.vgtDrawInitiator = drawInitiator;
  draw(*this, queue.vmId, 0, indexCount, 0, uConfig.vgtNumInstances,
       vgtIndexBase + indexOffset, maxSize);
  return true;
}
bool GraphicsPipe::writeData(Queue &queue) {
  auto len = rx::getBits(queue.rptr[0], 29, 16) - 1;
  auto control = queue.rptr[1];
  auto dstAddressLo = queue.rptr[2];
  auto dstAddressHi = queue.rptr[3];
  auto data = queue.rptr + 4;

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
  case 5: { // memory async
    auto address =
        (dstAddressLo & ~3) | (static_cast<std::uint64_t>(dstAddressHi) << 32);
    dstPointer = RemoteMemory{queue.vmId}.getPointer<std::uint32_t>(address);
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
bool GraphicsPipe::memSemaphore(Queue &queue) {
  // FIXME
  return true;
}
bool GraphicsPipe::waitRegMem(Queue &queue) {
  auto engine = rx::getBit(queue.rptr[1], 8);
  auto memSpace = rx::getBit(queue.rptr[1], 4);
  auto function = rx::getBits(queue.rptr[1], 2, 0);
  auto pollAddressLo = queue.rptr[2];
  auto pollAddressHi = queue.rptr[3] & ((1 << 16) - 1);
  auto reference = queue.rptr[4];
  auto mask = queue.rptr[5];
  auto pollInterval = queue.rptr[6];

  std::uint32_t pollData;

  if (memSpace == 0) {
    pollData = *getMmRegister(pollAddressLo & ((1 << 16) - 1));
  } else {
    auto pollAddress = (pollAddressLo & ~3) |
                       (static_cast<std::uint64_t>(pollAddressHi) << 32);
    pollData = *RemoteMemory{queue.vmId}.getPointer<std::uint32_t>(pollAddress);
  }

  return compare(function, pollData, mask, reference);
}
bool GraphicsPipe::indirectBuffer(Queue &queue) {
  rx::dieIf(queue.indirectLevel < 0, "unexpected indirect buffer from CP");

  auto addressLo = queue.rptr[1] & ~3;
  auto addressHi = queue.rptr[2] & ((1 << 16) - 1);
  auto vmId = queue.rptr[3] >> 24;
  auto ibSize = queue.rptr[4] & ((1 << 20) - 1);
  auto address = addressLo | (static_cast<std::uint64_t>(addressHi) << 32);

  auto rptr = RemoteMemory{queue.vmId}.getPointer<std::uint32_t>(address);
  setDeQueue(Queue::createFromRange(queue.vmId, rptr, ibSize),
             queue.indirectLevel + 1);
  return true;
}
bool GraphicsPipe::pfpSyncMe(Queue &queue) {
  // TODO
  return true;
}
bool GraphicsPipe::condWrite(Queue &queue) {
  auto writeSpace = rx::getBit(queue.rptr[1], 8);
  auto pollSpace = rx::getBit(queue.rptr[1], 4);
  auto function = rx::getBits(queue.rptr[1], 2, 0);
  auto pollAddressLo = queue.rptr[2];
  auto pollAddressHi = queue.rptr[3] & ((1 << 16) - 1);
  auto reference = queue.rptr[4];
  auto mask = queue.rptr[5];
  auto writeAddressLo = queue.rptr[6];
  auto writeAddressHi = queue.rptr[7] & ((1 << 16) - 1);
  auto writeData = queue.rptr[8];

  std::uint32_t pollData;

  if (pollSpace == 0) {
    pollData = *getMmRegister(pollAddressLo & ((1 << 16) - 1));
  } else {
    auto pollAddress = (pollAddressLo & ~3) |
                       (static_cast<std::uint64_t>(pollAddressHi) << 32);
    pollData = *RemoteMemory{queue.vmId}.getPointer<std::uint32_t>(pollAddress);
  }

  if (compare(function, pollData, mask, reference)) {
    if (writeSpace == 0) {
      *getMmRegister(writeAddressLo & ((1 << 16) - 1)) = writeData;
    } else {
      auto writeAddress = (writeAddressLo & ~3) |
                          (static_cast<std::uint64_t>(writeAddressHi) << 32);

      *RemoteMemory{queue.vmId}.getPointer<std::uint32_t>(writeAddress) =
          writeData;
    }
  }

  return true;
}

bool GraphicsPipe::eventWrite(Queue &queue) {
  enum {
    kEventZPassDone = 1,
    kEventSamplePipelineStat = 2,
    kEventSampleStreamOutStat = 3,
    kEventPartialFlush = 4,
  };

  auto eventCntl = queue.rptr[1];
  auto invL2 = rx::getBit(eventCntl, 20);
  auto eventIndex = rx::getBits(eventCntl, 11, 8);
  auto eventType = rx::getBits(eventCntl, 5, 0);

  context.vgtEventInitiator = eventType;

  if (eventIndex == kEventZPassDone || eventIndex == kEventSamplePipelineStat ||
      eventIndex == kEventSampleStreamOutStat) {
    auto addressLo = queue.rptr[2] & ~7;
    auto addressHi = queue.rptr[3] & ((1 << 16) - 1);
    auto address = addressLo | (static_cast<std::uint64_t>(addressHi) << 32);
    rx::die("unimplemented event write, event index %#x, address %lx",
            eventIndex, address);
    return true;
  }

  // FIXME
  return true;
}

bool GraphicsPipe::eventWriteEop(Queue &queue) {
  auto eventCntl = queue.rptr[1];
  auto addressLo = queue.rptr[2] & ~3;
  auto dataCntl = queue.rptr[3];
  auto dataLo = queue.rptr[4];
  auto dataHi = queue.rptr[5];

  auto invL2 = rx::getBit(eventCntl, 20);
  auto eventIndex = rx::getBits(eventCntl, 11, 8);
  auto eventType = rx::getBits(eventCntl, 5, 0);
  auto dataSel = rx::getBits(dataCntl, 31, 29);
  auto intSel = rx::getBits(dataCntl, 25, 24);
  auto addressHi = rx::getBits(dataCntl, 15, 0);

  auto address = addressLo | (static_cast<std::uint64_t>(addressHi) << 32);
  auto pointer = RemoteMemory{queue.vmId}.getPointer<std::uint64_t>(address);

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

  return true;
}

bool GraphicsPipe::eventWriteEos(Queue &queue) {
  auto eventCntl = queue.rptr[1];
  auto addressLo = queue.rptr[2] & ~3;
  auto cmdInfo = queue.rptr[3];
  auto dataInfo = queue.rptr[4];

  auto eventIndex = rx::getBits(eventCntl, 11, 8);
  auto eventType = rx::getBits(eventCntl, 5, 0);
  auto cmd = rx::getBits(cmdInfo, 31, 29);
  auto addressHi = rx::getBits(cmdInfo, 15, 0);

  auto address = addressLo | (static_cast<std::uint64_t>(addressHi) << 32);
  auto pointer = RemoteMemory{queue.vmId}.getPointer<std::uint32_t>(address);

  context.vgtEventInitiator = eventType;

  switch (cmd) {
  case 1: { // store GDS data to memory
    auto sizeDw = rx::getBits(dataInfo, 31, 16);
    auto gdsIndexDw = rx::getBits(dataInfo, 15, 0);
    rx::die("unimplemented event write eos gds data");
    break;
  }

  case 2: // after GDS writes confirm, store 32 bit DATA to memory as fence
    *pointer = dataInfo;
    break;

  default:
    rx::die("unexpected event write eos command: %#x", cmd);
  }
  return true;
}

bool GraphicsPipe::dmaData(Queue &queue) {
  // FIXME
  return true;
}

bool GraphicsPipe::setConfigReg(Queue &queue) {
  rx::dieIf(queue.indirectLevel != 0, "setConfigReg from queue %d",
            queue.indirectLevel);

  auto len = rx::getBits(queue.rptr[0], 29, 16);
  auto offset = queue.rptr[1] & 0xffff;
  auto data = queue.rptr + 2;

  rx::dieIf(
      (offset + len) * sizeof(std::uint32_t) > sizeof(device->config),
      "out of Config regs, offset: %x, count %u, %s\n", offset, len,
      gnm::mmio::registerName(decltype(device->config)::kMmioOffset + offset));

  std::memcpy(reinterpret_cast<std::uint32_t *>(&device->config) + offset, data,
              sizeof(std::uint32_t) * len);

  return true;
}

bool GraphicsPipe::setShReg(Queue &queue) {
  auto len = rx::getBits(queue.rptr[0], 29, 16);
  auto offset = queue.rptr[1] & 0xffff;
  auto index = queue.rptr[1] >> 26;
  auto data = queue.rptr + 2;

  rx::dieIf((offset + len) * sizeof(std::uint32_t) > sizeof(sh),
            "out of SH regs, offset: %x, count %u, %s\n", offset, len,
            gnm::mmio::registerName(decltype(sh)::kMmioOffset + offset));

  std::memcpy(reinterpret_cast<std::uint32_t *>(&sh) + offset, data,
              sizeof(std::uint32_t) * len);

  return true;
}

bool GraphicsPipe::setUConfigReg(Queue &queue) {
  auto len = rx::getBits(queue.rptr[0], 29, 16);
  auto offset = queue.rptr[1] & 0xffff;
  auto index = queue.rptr[1] >> 26;
  auto data = queue.rptr + 2;

  if (index != 0) {
    std::fprintf(
        stderr,
        "set UConfig regs with index, offset: %x, count %u, index %u, %s\n",
        offset, len, index,
        gnm::mmio::registerName(decltype(uConfig)::kMmioOffset + offset));

    for (std::size_t i = 0; i < len; ++i) {
      std::fprintf(
          stderr, "writing to %s value %x\n",
          gnm::mmio::registerName(decltype(uConfig)::kMmioOffset + offset + i),
          data[i]);
    }
  }

  rx::dieIf((offset + len) * sizeof(std::uint32_t) > sizeof(context),
            "out of UConfig regs, offset: %u, count %u, %s\n", offset, len,
            gnm::mmio::registerName(decltype(uConfig)::kMmioOffset + offset));

  std::memcpy(reinterpret_cast<std::uint32_t *>(&uConfig) + offset, data,
              sizeof(std::uint32_t) * len);

  return true;
}

bool GraphicsPipe::setContextReg(Queue &queue) {
  auto len = rx::getBits(queue.rptr[0], 29, 16);
  auto offset = queue.rptr[1] & 0xffff;
  auto index = queue.rptr[1] >> 26;
  auto data = queue.rptr + 2;

  if (index != 0) {
    std::fprintf(
        stderr,
        "set Context regs with index, offset: %x, count %u, index %u, %s\n",
        offset, len, index,
        gnm::mmio::registerName(decltype(context)::kMmioOffset + offset));

    for (std::size_t i = 0; i < len; ++i) {
      std::fprintf(
          stderr, "writing to %s value %x\n",
          gnm::mmio::registerName(decltype(context)::kMmioOffset + offset + i),
          data[i]);
    }
  }

  rx::dieIf((offset + len) * sizeof(std::uint32_t) > sizeof(context),
            "out of Context regs, offset: %u, count %u, %s\n", offset, len,
            gnm::mmio::registerName(decltype(context)::kMmioOffset + offset));

  std::memcpy(reinterpret_cast<std::uint32_t *>(&context) + offset, data,
              sizeof(std::uint32_t) * len);

  // for (std::size_t i = 0; i < len; ++i) {
  //   std::fprintf(stderr,
  //       "writing to %s value %x\n",
  //       gnm::mmio::registerName(decltype(context)::kMmioOffset + offset + i),
  //       data[i]);
  // }
  return true;
}

bool GraphicsPipe::setCeDeCounters(Queue &queue) {
  auto counterLo = queue.rptr[1];
  auto counterHi = queue.rptr[2];
  auto counter = counterLo | (static_cast<std::uint64_t>(counterHi) << 32);
  deCounter = counter;
  ceCounter = counter;
  return true;
}

bool GraphicsPipe::waitOnCeCounter(Queue &queue) {
  auto counterLo = queue.rptr[1];
  auto counterHi = queue.rptr[2];
  auto counter = counterLo | (static_cast<std::uint64_t>(counterHi) << 32);
  return deCounter >= counter;
}

bool GraphicsPipe::waitOnDeCounterDiff(Queue &queue) {
  auto waitDiff = queue.rptr[1];
  auto diff = ceCounter - deCounter;
  return diff < waitDiff;
}

bool GraphicsPipe::incrementCeCounter(Queue &) {
  ceCounter++;
  return true;
}

bool GraphicsPipe::incrementDeCounter(Queue &) {
  deCounter++;
  return true;
}

bool GraphicsPipe::loadConstRam(Queue &queue) {
  std::uint32_t addressLo = queue.rptr[1];
  std::uint32_t addressHi = queue.rptr[2];
  std::uint32_t numDw = queue.rptr[3] & ((1 << 15) - 1);
  std::uint32_t offset =
      (queue.rptr[4] & ((1 << 16) - 1)) / sizeof(std::uint32_t);
  auto address = addressLo | (static_cast<std::uint64_t>(addressHi) << 32);
  std::memcpy(constantMemory + offset,
              RemoteMemory{queue.vmId}.getPointer(address),
              numDw * sizeof(std::uint32_t));

  return true;
}

bool GraphicsPipe::writeConstRam(Queue &queue) {
  std::uint32_t offset =
      (queue.rptr[1] & ((1 << 16) - 1)) / sizeof(std::uint32_t);
  std::uint32_t data = queue.rptr[2];
  std::memcpy(constantMemory + offset, &data, sizeof(std::uint32_t));
  return true;
}

bool GraphicsPipe::dumpConstRam(Queue &queue) {
  std::uint32_t offset =
      (queue.rptr[1] & ((1 << 16) - 1)) / sizeof(std::uint32_t);
  std::uint32_t numDw = queue.rptr[2] & ((1 << 15) - 1);
  std::uint32_t addressLo = queue.rptr[3];
  std::uint32_t addressHi = queue.rptr[4];
  auto address = addressLo | (static_cast<std::uint64_t>(addressHi) << 32);
  std::memcpy(RemoteMemory{queue.vmId}.getPointer(address),
              constantMemory + offset, numDw * sizeof(std::uint32_t));

  return true;
}

bool GraphicsPipe::unknownPacket(Queue &queue) {
  auto op = rx::getBits(queue.rptr[0], 15, 8);

  rx::die("unimplemented gfx pm4 packet: %s, queue %u\n",
          gnm::pm4OpcodeToString(op), queue.indirectLevel);
}
