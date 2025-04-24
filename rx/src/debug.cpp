#include "debug.hpp"
#include <fstream>
#include <list>
#include <print>
#include <thread>
#include <vector>

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#ifdef _WIN32
#include <windows.h>
#else

#ifdef __linux__
#include <linux/limits.h>
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

  std::println(stderr, "waiting for debugger, pid {}", ::getpid());

  while (!isDebuggerPresent()) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  std::println(stderr, "debugger was attached");
  std::this_thread::sleep_for(std::chrono::seconds(3));
  breakpoint();
}

void rx::runDebugger() {
#ifdef __linux__
  int pid = ::getpid();
  char path[PATH_MAX];
  ::readlink("/proc/self/exe", path, sizeof(path));
  if (fork()) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    waitForDebugger();
    return;
  }

  auto pidString = std::to_string(pid);
  const char *gdbPath = "/usr/bin/gdb";

  std::list<std::string> storage;
  std::vector<const char *> argv;
  argv.push_back(gdbPath);
  argv.push_back(path);
  argv.push_back(pidString.c_str());
  argv.push_back("-iex");
  argv.push_back("set pagination off");
  argv.push_back("-ex");
  argv.push_back("handle SIGSYS nostop noprint");
  argv.push_back("-ex");
  argv.push_back("handle SIGUSR1 nostop noprint");
  // TODO: collect elfs
  //   argv.push_back("-ex");
  //   argv.push_back("add-symbol-file <path to elf> 0x400000");
  argv.push_back(nullptr);

  execv(gdbPath, (char **)argv.data());
#endif
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
