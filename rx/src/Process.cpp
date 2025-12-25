#include <rx/Process.hpp>

#include <vector>
#include <memory>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <sys/wait.h>
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

rx::ProcessId rx::spawn(const std::string &executable,
                        const std::vector<std::string> &args) {
#ifdef _WIN32
  // Build command line
  std::string cmdLine = "\"" + executable + "\"";
  for (const auto &arg : args) {
    cmdLine += " \"" + arg + "\"";
  }

  // Convert to wide string
  int wideSize =
      ::MultiByteToWideChar(CP_UTF8, 0, cmdLine.c_str(), -1, nullptr, 0);
  if (wideSize <= 0) {
    return static_cast<ProcessId>(-1);
  }

  std::vector<wchar_t> wideCmdLine(wideSize);
  ::MultiByteToWideChar(CP_UTF8, 0, cmdLine.c_str(), -1, wideCmdLine.data(),
                        wideSize);

  STARTUPINFOW si{};
  si.cb = sizeof(si);
  PROCESS_INFORMATION pi{};

  if (!::CreateProcessW(nullptr, wideCmdLine.data(), nullptr, nullptr, FALSE, 0,
                        nullptr, nullptr, &si, &pi)) {
    return static_cast<ProcessId>(-1);
  }

  ProcessId pid = ::GetProcessId(pi.hProcess);
  ::CloseHandle(pi.hThread);
  ::CloseHandle(pi.hProcess);

  return pid;
#else
  // fork + execv
  pid_t pid = ::fork();
  if (pid < 0) {
    return static_cast<ProcessId>(-1);
  }

  if (pid == 0) {
    // child process
    std::vector<char *> argv;
    argv.reserve(args.size() + 2);

    argv.push_back(const_cast<char *>(executable.c_str()));

    // Add arguments
    for (const auto &arg : args) {
      argv.push_back(const_cast<char *>(arg.c_str()));
    }

    argv.push_back(nullptr);

    ::execv(executable.c_str(), argv.data());
    // If execv returns, it failed
    ::_exit(127);
  }

  // parent process
  return static_cast<ProcessId>(pid);
#endif
}

bool rx::waitProcess(ProcessId pid, int *exitCode) {
#ifdef _WIN32
  HANDLE hProcess = ::OpenProcess(SYNCHRONIZE | PROCESS_QUERY_INFORMATION,
                                  FALSE, static_cast<DWORD>(pid));
  if (!hProcess) {
    return false;
  }

  DWORD result = ::WaitForSingleObject(hProcess, INFINITE);
  if (result != WAIT_OBJECT_0) {
    ::CloseHandle(hProcess);
    return false;
  }

  if (exitCode) {
    DWORD code = 0;
    if (::GetExitCodeProcess(hProcess, &code)) {
      *exitCode = static_cast<int>(code);
    } else {
      *exitCode = -1;
    }
  }

  ::CloseHandle(hProcess);
  return true;
#else
  int status = 0;
  pid_t result = ::waitpid(static_cast<pid_t>(pid), &status, 0);
  if (result == -1) {
    return false;
  }

  if (exitCode) {
    if (WIFEXITED(status)) {
      *exitCode = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
      *exitCode = 128 + WTERMSIG(status);
    } else {
      *exitCode = -1;
    }
  }

  return WIFEXITED(status) || WIFSIGNALED(status);
#endif
}

bool rx::suspendProcess(ProcessId pid) {
#ifdef _WIN32
  // Use NtSuspendProcess from ntdll.dll
  using NtSuspendProcessFn = long(__stdcall *)(HANDLE);
  static NtSuspendProcessFn ntSuspend = [] {
    if (auto mod = ::GetModuleHandleW(L"ntdll.dll")) {
      return reinterpret_cast<NtSuspendProcessFn>(
          ::GetProcAddress(mod, "NtSuspendProcess"));
    }
    return static_cast<NtSuspendProcessFn>(nullptr);
  }();

  if (!ntSuspend) {
    return false;
  }

  HANDLE hProcess =
      ::OpenProcess(PROCESS_SUSPEND_RESUME, FALSE, static_cast<DWORD>(pid));
  if (!hProcess) {
    return false;
  }

  long status = ntSuspend(hProcess);
  ::CloseHandle(hProcess);
  return status >= 0;
#else
  return ::kill(static_cast<pid_t>(pid), SIGSTOP) == 0;
#endif
}

bool rx::resumeProcess(ProcessId pid) {
#ifdef _WIN32
  // Use NtResumeProcess from ntdll.dll
  using NtResumeProcessFn = long(__stdcall *)(HANDLE);
  static NtResumeProcessFn ntResume = [] {
    if (auto mod = ::GetModuleHandleW(L"ntdll.dll")) {
      return reinterpret_cast<NtResumeProcessFn>(
          ::GetProcAddress(mod, "NtResumeProcess"));
    }
    return static_cast<NtResumeProcessFn>(nullptr);
  }();

  if (!ntResume) {
    return false;
  }

  HANDLE hProcess =
      ::OpenProcess(PROCESS_SUSPEND_RESUME, FALSE, static_cast<DWORD>(pid));
  if (!hProcess) {
    return false;
  }

  long status = ntResume(hProcess);
  ::CloseHandle(hProcess);
  return status >= 0;
#else
  return ::kill(static_cast<pid_t>(pid), SIGCONT) == 0;
#endif
}
