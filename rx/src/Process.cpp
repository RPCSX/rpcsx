#include <rx/Process.hpp>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#endif

#ifdef __APPLE__
#include <pthread.h>
#endif

rx::ProcessId rx::getCurrentProcessId() {
#ifdef _WIN32
  return ::GetCurrentProcessId();
#else
  return ::getpid();
#endif
}

rx::ThreadId rx::getCurrentThreadId() {
#ifdef _WIN32
  return ::GetCurrentProcessId();
#elif defined(__APPLE__)
  uint64_t tid = 0;
  pthread_threadid_np(nullptr, &tid);
  return tid;
#else
  return ::gettid();
#endif
}

rx::ProcessHandle rx::getCurrentProcessHandle() {
#ifdef _WIN32
  return ::GetCurrentProcess();
#else
  return getCurrentProcessId();
#endif
}

rx::ThreadHandle rx::getCurrentThreadHandle() {
#ifdef _WIN32
  return ::GetCurrentThread();
#elif defined(__APPLE__)
  return pthread_self();
#else
  return getCurrentThreadId();
#endif
}
