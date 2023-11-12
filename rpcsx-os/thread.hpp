#pragma once

#include "orbis/thread/Thread.hpp"

namespace rx::thread {
void initialize();
void deinitialize();
void setupSignalStack();
void setupThisThread();

void invoke(orbis::Thread *thread);
} // namespace rx::thread
