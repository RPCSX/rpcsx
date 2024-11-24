#include "debug.hpp"
#include <fstream>
#include <list>
#include <print>
#include <thread>
#include <vector>

#ifdef __GNUC__
#include <linux/limits.h>
#include <sys/ptrace.h>
#include <unistd.h>

bool rx::isDebuggerPresent() {
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
}

#else
bool rx::isDebuggerPresent() { return false; }
void rx::waitForDebugger() {}
void rx::runDebugger() {}
#endif

void rx::breakpoint() {
#if __has_builtin(__builtin_debugtrap)
  __builtin_debugtrap();
#elif defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__))
  __asm__ volatile("int3");
#endif
}

void rx::breakpointIfDebugging() {
  if (isDebuggerPresent()) {
    breakpoint();
  }
}
