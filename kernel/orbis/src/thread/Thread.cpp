#include "thread/Thread.hpp"
#include "thread/Process.hpp"

orbis::Thread::~Thread() {
  Thread::Storage::DestructAll(storage);

  auto size = sizeof(Thread);
  size = rx::alignUp(size, Thread::Storage::GetAlignment());
  size += Thread::Storage::GetSize();

  kfree(this, size);
}

orbis::Thread *orbis::createThread(Process *process, std::string_view name) {
  auto size = sizeof(Thread);
  size = rx::alignUp(size, Thread::Storage::GetAlignment());
  auto storageOffset = size;
  size += Thread::Storage::GetSize();

  auto memory = (std::byte *)kalloc(
      size,
      std::max<std::size_t>(alignof(Thread), Thread::Storage::GetAlignment()));

  auto result = new (memory) Thread();
  result->storage = memory + storageOffset;
  result->tproc = process;
  result->name = name;

  Thread::Storage::ConstructAll(result->storage);

  std::lock_guard lock(process->mtx);
  auto baseId = process->threadsMap.insert(result);
  result->tproc = process;
  result->tid = process->pid + baseId;
  result->state = orbis::ThreadState::RUNNING;

  return result;
}

uintptr_t orbis::getCallerAddress(Thread *thread) {
  if (!thread->context) {
    return 0;
  }

  auto rbp = readRegister(thread->context, RegisterId::rbp);
  std::uint64_t result = 0;

  while (rbp < 0x8000'0000'0000) {
    auto framePtr = std::bit_cast<ptr<uint64_t>>(rbp);

    std::uint64_t nextFrame;
    std::uint64_t retAddress;
    if (orbis::uread(retAddress, framePtr + 1) != orbis::ErrorCode{}) {
      break;
    }

    if (orbis::uread(nextFrame, framePtr) != orbis::ErrorCode{}) {
      break;
    }

    if (!thread->tproc->libkernelRange.contains(retAddress)) {
      result = retAddress;
      break;
    }

    rbp = nextFrame;
  }

  return result;
}
