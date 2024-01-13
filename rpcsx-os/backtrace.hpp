#pragma once

#include <cstddef>
#include <cstdint>
#include <ucontext.h>

namespace orbis {
struct Thread;
}

namespace rx {
std::size_t printAddressLocation(char *dest, std::size_t destLen,
                                 orbis::Thread *thread, std::uint64_t address);
void printStackTrace(ucontext_t *context, int fileno);
void printStackTrace(ucontext_t *context, orbis::Thread *thread, int fileno);
} // namespace rx
