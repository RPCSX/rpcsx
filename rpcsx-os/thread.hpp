#pragma once

#include "orbis/thread/Thread.hpp"

namespace rx::thread {
void initialize();
void deinitialize();

extern thread_local orbis::Thread *g_current;
void invoke(orbis::Thread *thread);
} // namespace rx::thread
