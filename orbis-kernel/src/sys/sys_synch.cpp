#include "sys/sysproto.hpp"
#include <thread>

orbis::SysResult orbis::sys_yield(Thread *thread) {
    std::this_thread::yield();
    return {};
}
