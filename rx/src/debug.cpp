#include "debug.hpp"
#include "Process.hpp"
#include "filesystem.hpp"
#include "print.hpp"
#include <fstream>
#include <thread>
#include <vector>

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#ifdef _WIN32
#include <windows.h>
#else

#ifdef __linux__
#include <sys/ptrace.h>
#endif
#include <unistd.h>

#endif

bool rx::isDebuggerPresent() {
#ifdef _WIN32
  return ::IsDebuggerPresent();
#elif defined(__APPLE__) || defined(__DragonFly__) || defined(__FreeBSD__) ||  \
    defined(__NetBSD__) || defined(__OpenBSD__)
  int mib[] = {
      CTL_KERN,
      KERN_PROC,
      KERN_PROC_PID,
      getpid(),
#if defined(__NetBSD__) || defined(__OpenBSD__)
      sizeof(struct kinfo_proc),
      1,
#endif
  };
  u_int miblen = std::size(mib);
  struct kinfo_proc info;
  usz size = sizeof(info);

  if (sysctl(mib, miblen, &info, &size, NULL, 0)) {
    return false;
  }

  return info.KP_FLAGS & P_TRACED;
#elif defined(__linux__)
  std::ifstream in("/proc/self/status");
  std::string line;
  while (std::getline(in, line)) {
    const std::string_view tracerPrefix = "TracerPid:\t";

    if (!line.starts_with(tracerPrefix)) {
      continue;
    }

    std::string_view tracer = line;
    tracer.remove_prefix(tracerPrefix.size());

    if (tracer.size() == 1 && tracer[0] == '0')
      return false;

    return true;
  }

  return false;
#endif
}

void rx::waitForDebugger() {
  if (isDebuggerPresent()) {
    return;
  }

  rx::println(stderr, "waiting for debugger, pid {}", getCurrentProcessId());

  while (!isDebuggerPresent()) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  rx::println(stderr, "debugger was attached");
  std::this_thread::sleep_for(std::chrono::seconds(3));
  breakpoint();
}

void rx::runDebugger() {
  auto pid = getCurrentProcessId();
  auto path = rx::getExecutablePath();

  auto pidString = std::to_string(pid);

  // Find gdb in PATH
  auto gdbPath = rx::findExecutable("gdb");

  if (gdbPath.empty()) {
    return;
  }

  std::vector<std::string> args = {
      path,
      pidString,
      "-iex",
      "set pagination off",
      "-ex",
      "handle SIGSYS nostop noprint",
      "-ex",
      "handle SIGUSR1 nostop noprint"
      // TODO: collect elfs
      //   "-ex", "add-symbol-file <path to elf> 0x400000"
  };

  ProcessId debuggerPid = rx::spawn(gdbPath, args);
  if (debuggerPid != static_cast<ProcessId>(-1)) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    waitForDebugger();
  }
}

void rx::breakpoint() {
#if __has_builtin(__builtin_debugtrap)
  __builtin_debugtrap();
#elif defined(__GNUC__)
#if defined(__i386__) || defined(__x86_64__)
  __asm__ volatile("int3");
#elif defined(__aarch64__) || defined(__arm64__) || defined(_M_ARM64)
  __asm__ volatile("brk 0x42");
#endif
#elif defined(_M_X64)
  __debugbreak();
#endif
}

void rx::breakpointIfDebugging() {
  if (isDebuggerPresent()) {
    breakpoint();
  }
}
