#include "rx/watchdog.hpp"
#include "gpu/Device.hpp"
#include "orbis/KernelContext.hpp"
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <filesystem>
#include <format>
#include <print>
#include <string_view>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#include <utility>

static std::atomic<bool> g_exitRequested;
static std::atomic<bool> g_runGpuRequested;
static pid_t g_watchdogPid;
static pid_t g_gpuPid;
static char g_shmPath[256];

enum class MessageId {
  RunGPU,
};

static void runGPU() {
  if (g_gpuPid != 0 || orbis::g_context.gpuDevice != nullptr) {
    return;
  }

  auto childPid = ::fork();

  if (childPid != 0) {
    g_gpuPid = childPid;
    return;
  }

  amdgpu::Device *gpu;
  {
    pthread_setname_np(pthread_self(), "rpcsx-gpu");
    std::lock_guard lock(orbis::g_context.gpuDeviceMtx);
    if (orbis::g_context.gpuDevice != nullptr) {
      std::exit(0);
    }

    int logFd =
        ::open("log-gpu.txt", O_CREAT | O_RDWR | O_TRUNC, S_IRUSR | S_IWUSR);
    dup2(logFd, 1);
    dup2(logFd, 2);
    ::close(logFd);

    gpu = orbis::knew<amdgpu::Device>();
    orbis::g_context.gpuDevice = gpu;
  }

  gpu->start();
  std::exit(0);
}

static void handleManagementSignal(siginfo_t *info) {
  switch (static_cast<MessageId>(info->si_value.sival_int)) {
  case MessageId::RunGPU:
    g_runGpuRequested = true;
    break;
  }
}

static void handle_watchdog_signal(int sig, siginfo_t *info, void *) {
  if (sig == SIGUSR1) {
    handleManagementSignal(info);
  }

  if (sig == SIGINT || sig == SIGQUIT) {
    g_exitRequested = true;
  }
}

static void sendMessage(MessageId id) {
  sigqueue(g_watchdogPid, SIGUSR1,
           {
               .sival_int = static_cast<int>(id),
           });
}

const char *rx::getShmPath() { return g_shmPath; }
std::filesystem::path rx::getShmGuestPath(std::string_view path) {
  return std::format("{}/guest/{}", getShmPath(), path);
}

void rx::createGpuDevice() { sendMessage(MessageId::RunGPU); }
void rx::shutdown() { kill(g_watchdogPid, SIGQUIT); }

static void killProcesses(std::vector<int> list) {
  int iteration = 0;
  while (!list.empty()) {
    auto signal = iteration++ > 20 ? SIGKILL : SIGQUIT;

    for (std::size_t i = 0; i < list.size();) {
      if (list[i] == 0 || ::kill(list[i], signal) != 0) {
        if (i + 1 < list.size()) {
          std::swap(list[i], list.back());
        }

        list.pop_back();
        continue;
      }

      ++i;
    }

    if (signal == SIGKILL) {
      break;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
}

int rx::startWatchdog() {
  auto watchdogPid = ::getpid();
  g_watchdogPid = watchdogPid;
  std::format_to(g_shmPath, "/dev/shm/rpcsx/{}", watchdogPid);

  if (!std::filesystem::create_directories(g_shmPath)) {
    perror("failed to create shared memory directory");
    std::exit(-1);
  }

  if (!std::filesystem::create_directory(std::format("{}/guest", g_shmPath))) {
    perror("failed to create guest shared memory directory");
    std::exit(-1);
  }

  pid_t initProcessPid = fork();

  if (initProcessPid == 0) {
    return watchdogPid;
  }

  pthread_setname_np(pthread_self(), "rpcsx-watchdog");

  struct sigaction act{};
  act.sa_sigaction = handle_watchdog_signal;
  act.sa_flags = SA_SIGINFO;

  if (sigaction(SIGUSR1, &act, nullptr)) {
    perror("Error sigaction:");
    std::exit(-1);
  }

  if (sigaction(SIGINT, &act, nullptr)) {
    perror("Error sigaction:");
    std::exit(-1);
  }

  if (sigaction(SIGQUIT, &act, nullptr)) {
    perror("Error sigaction:");
    std::exit(-1);
  }

  int stat = 0;
  while (true) {
    auto childPid = wait(&stat);

    if (g_exitRequested == true) {
      break;
    }

    if (childPid == initProcessPid) {
      initProcessPid = 0;
      break;
    }

    if (childPid == g_gpuPid) {
      g_gpuPid = 0;
      // FIXME: Restart GPU?
      break;
    }

    if (g_runGpuRequested) {
      std::println("watchdog: gpu start requested");
      g_runGpuRequested = false;
      runGPU();
    }
  }

  std::filesystem::remove_all(g_shmPath);
  killProcesses({initProcessPid, g_gpuPid});
  ::wait(nullptr);
  std::_Exit(stat);
}
