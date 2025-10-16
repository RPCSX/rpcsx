#pragma once

#include <cstdint>

namespace rx {
using ProcessId = std::uint32_t;

#ifdef __APPLE__
using ThreadId = std::uint64_t;
#else
using ThreadId = ProcessId;
#endif

#ifdef _WIN32
using ProcessHandle = void *;
#else
using ProcessHandle = ProcessId;
#endif

#if defined(_WIN32) || defined(__APPLE__)
using ThreadHandle = void *;
#else
using ThreadHandle = ThreadId;
#endif

ProcessId getCurrentProcessId();
ThreadId getCurrentThreadId();
ProcessHandle getCurrentProcessHandle();
ThreadHandle getCurrentThreadHandle();
} // namespace rx
