#pragma once

#include <cstdint>
#include <string>
#include <vector>

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

// Spawn a new process with the given executable and arguments
// Returns the process ID on success, or -1 on failure
ProcessId spawn(const std::string &executable,
                const std::vector<std::string> &args = {});

// Wait for a process to exit and return its exit code
// Returns true if the process exited normally, false otherwise
bool waitProcess(ProcessId pid, int *exitCode = nullptr);

// Suspend a process
// Returns true on success, false otherwise
bool suspendProcess(ProcessId pid);

// Resume a suspended process
// Returns true on success, false otherwise
bool resumeProcess(ProcessId pid);
} // namespace rx
