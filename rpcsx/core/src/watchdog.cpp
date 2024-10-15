#include "rx/watchdog.hpp"
#include "gpu/DeviceCtl.hpp"
#include "orbis/KernelContext.hpp"
#include <bit>
#include <chrono>
#include <csignal>
#include <cstdint>
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
static std::vector<int> g_attachedProcesses;

enum class MessageId {
  RunGPU,
  AttachProcess,
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

  amdgpu::DeviceCtl gpu;
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

    gpu = amdgpu::DeviceCtl::createDevice();
    orbis::g_context.gpuDevice = gpu.getOpaque();
  }

  gpu.start();
  std::exit(0);
}

static void handleManagementSignal(siginfo_t *info) {
  auto rawMessage = std::bit_cast<std::uintptr_t>(info->si_value.sival_ptr);
  auto id = static_cast<MessageId>(static_cast<std::uint32_t>(rawMessage));
  auto data = static_cast<std::uint32_t>(rawMessage >> 32);
  switch (id) {
  case MessageId::RunGPU:
    g_runGpuRequested = true;
    break;
  case MessageId::AttachProcess:
    g_attachedProcesses.push_back(data);
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

static void sendMessage(MessageId id, std::uint32_t data) {
  sigqueue(g_watchdogPid, SIGUSR1,
           {
               .sival_ptr = std::bit_cast<void *>(
                   ((static_cast<std::uintptr_t>(data) << 32) |
                    static_cast<std::uintptr_t>(id))),
           });
}

const char *rx::getShmPath() { return g_shmPath; }
std::filesystem::path rx::getShmGuestPath(std::string_view path) {
  return std::format("{}/guest/{}", getShmPath(), path);
}

void rx::createGpuDevice() { sendMessage(MessageId::RunGPU, 0); }
void rx::shutdown() { kill(g_watchdogPid, SIGQUIT); }

void rx::attachProcess(int pid) { sendMessage(MessageId::AttachProcess, pid); }

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

  if (sigaction(SIGHUP, &act, nullptr)) {
    perror("Error sigaction:");
    std::exit(-1);
  }

  sigset_t sigSet;
  sigemptyset(&sigSet);
  sigaddset(&sigSet, SIGUSR1);
  sigaddset(&sigSet, SIGINT);
  sigaddset(&sigSet, SIGQUIT);
  sigaddset(&sigSet, SIGHUP);

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

    if (childPid <= 0) {
      continue;
    }

    pthread_sigmask(SIG_BLOCK, &sigSet, nullptr);
    for (std::size_t i = 0; i < g_attachedProcesses.size();) {
      if (g_attachedProcesses[i] != childPid) {
        continue;
      }

      if (i + 1 != g_attachedProcesses.size()) {
        std::swap(g_attachedProcesses[i], g_attachedProcesses.back());
      }
      g_attachedProcesses.pop_back();
    }
    pthread_sigmask(SIG_UNBLOCK, &sigSet, nullptr);
  }

  pthread_sigmask(SIG_BLOCK, &sigSet, nullptr);

  std::filesystem::remove_all(g_shmPath);
  g_attachedProcesses.push_back(initProcessPid);
  g_attachedProcesses.push_back(g_gpuPid);
  killProcesses(g_attachedProcesses);
  ::wait(nullptr);
  std::_Exit(stat);
}
