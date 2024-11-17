#pragma once

#include "orbis/thread/Thread.hpp"

namespace rx::thread {
std::size_t getSigAltStackSize();
void initialize();
void deinitialize();
void *setupSignalStack();
void *setupSignalStack(void *address);
void setupThisThread();

void copyContext(orbis::MContext &dst, const mcontext_t &src);
void copyContext(orbis::Thread *thread, orbis::UContext &dst,
                 const ucontext_t &src);
void invoke(orbis::Thread *thread);
} // namespace rx::thread
