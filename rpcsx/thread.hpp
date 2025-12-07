#pragma once

#include "orbis/thread/Thread.hpp"
#include <csignal>

namespace rx::thread {
std::size_t getSigAltStackSize();
void initialize();
void deinitialize();
void *setupSignalStack();
void *setupSignalStack(void *address);
void setupThisThread();

void copyContext(orbis::MContext &dst, const mcontext_t &src,
                 std::uint64_t addr = 0);
void copyContext(orbis::Thread *thread, orbis::UContext &dst,
                 const ucontext_t &src, std::uint64_t addr = 0);
bool invokeSignalHandler(orbis::Thread *thread, siginfo_t *siginfo, int signo,
                         ucontext_t *context = nullptr);
void setContext(orbis::Thread *thread, const orbis::UContext &src);
void invoke(orbis::Thread *thread);
} // namespace rx::thread
