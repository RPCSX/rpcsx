#include "orbis/sys/syscall.hpp"
#include "orbis/thread/Process.hpp"
#include "orbis/thread/ProcessOps.hpp"
#include <concepts>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>

#include <orbis/KernelContext.hpp>
#include <orbis/error.hpp>
#include <orbis/sys/sysentry.hpp>
#include <orbis/thread/Thread.hpp>
#include <thread>
#include <utility>

struct Registers {
  std::uint64_t r15;
  std::uint64_t r14;
  std::uint64_t r13;
  std::uint64_t r12;
  std::uint64_t r11;
  std::uint64_t r10;
  std::uint64_t r9;
  std::uint64_t r8;
  std::uint64_t rdi;
  std::uint64_t rsi;
  std::uint64_t rbp;
  std::uint64_t rbx;
  std::uint64_t rdx;
  std::uint64_t rcx;
  std::uint64_t rax;
  std::uint64_t rsp;
  std::uint64_t rflags;
};

namespace orbis {
uint64_t readRegister(void *context, RegisterId id) {
  auto c = reinterpret_cast<Registers *>(context);
  switch (id) {
  case RegisterId::r15: return c->r15;
  case RegisterId::r14: return c->r14;
  case RegisterId::r13: return c->r13;
  case RegisterId::r12: return c->r12;
  case RegisterId::r11: return c->r11;
  case RegisterId::r10: return c->r10;
  case RegisterId::r9: return c->r9;
  case RegisterId::r8: return c->r8;
  case RegisterId::rdi: return c->rdi;
  case RegisterId::rsi: return c->rsi;
  case RegisterId::rbp: return c->rbp;
  case RegisterId::rbx: return c->rbx;
  case RegisterId::rdx: return c->rdx;
  case RegisterId::rcx: return c->rcx;
  case RegisterId::rax: return c->rax;
  case RegisterId::rsp: return c->rsp;
  case RegisterId::rflags: return c->rflags;
  }
}

void writeRegister(void *context, RegisterId id, uint64_t value) {
  auto c = reinterpret_cast<Registers *>(context);
  switch (id) {
  case RegisterId::r15: c->r15 = value; return;
  case RegisterId::r14: c->r14 = value; return;
  case RegisterId::r13: c->r13 = value; return;
  case RegisterId::r12: c->r12 = value; return;
  case RegisterId::r11: c->r11 = value; return;
  case RegisterId::r10: c->r10 = value; return;
  case RegisterId::r9: c->r9 = value; return;
  case RegisterId::r8: c->r8 = value; return;
  case RegisterId::rdi: c->rdi = value; return;
  case RegisterId::rsi: c->rsi = value; return;
  case RegisterId::rbp: c->rbp = value; return;
  case RegisterId::rbx: c->rbx = value; return;
  case RegisterId::rdx: c->rdx = value; return;
  case RegisterId::rcx: c->rcx = value; return;
  case RegisterId::rax: c->rax = value; return;
  case RegisterId::rsp: c->rsp = value; return;
  case RegisterId::rflags: c->rflags = value; return;
  }
}
}

static thread_local orbis::Thread *g_guestThread = nullptr;

class CPU {
  struct Task {
    orbis::Thread *thread;
    std::function<void()> job;
  };

  int m_index;
  std::deque<Task> m_workQueue;
  std::condition_variable m_cv;
  std::mutex m_mtx;
  std::atomic<bool> m_terminate_flag{false};
  std::thread m_hostThread{[this] { entry(); }};

public:
  CPU(int index) : m_index(index) {}

  int getIndex() const {
    return m_index;
  }

  void addTask(orbis::Thread *thread, std::function<void()> task) {
    m_workQueue.push_back({thread, std::move(task)});
    m_cv.notify_one();
  }

  ~CPU() {
    m_terminate_flag = true;
    m_cv.notify_all();
    m_hostThread.join();
  }

  void entry() {
    while (!m_terminate_flag) {
      Task task;
      {
        std::unique_lock lock(m_mtx);
        m_cv.wait(lock,
                  [this] { return !m_workQueue.empty() || m_terminate_flag; });

        if (m_terminate_flag) {
          break;
        }

        task = std::move(m_workQueue.front());
        m_workQueue.pop_front();
      }

      task.thread->state = orbis::ThreadState::RUNNING;
      g_guestThread = task.thread;
      task.job();
      if (task.thread->state == orbis::ThreadState::RUNNING) {
        task.thread->state = orbis::ThreadState::INACTIVE;
      }
    }
  }
};

class Event {
  std::condition_variable m_cv;
  std::mutex m_mtx;
  std::atomic<bool> m_fired;

public:
  void wait() {
    std::unique_lock lock(m_mtx);
    m_cv.wait(lock, [&] { return m_fired == true; });
    m_fired = false;
  }

  void fire() {
    m_fired = true;
    m_cv.notify_all();
  }
};

struct orbis::ProcessOps procOps = {
  .exit = [](orbis::Thread *, orbis::sint status) -> orbis::SysResult {
    std::printf("sys_exit(%u)\n", status);
    std::exit(status);
  }
};

static orbis::Thread *allocateGuestThread(orbis::Process *process,
                                   orbis::lwpid_t tid) {
  auto guestThread = new orbis::Thread{};
  guestThread->state = orbis::ThreadState::RUNQ;
  guestThread->tid = tid;
  guestThread->tproc = process;
  return guestThread;
}


static void onSysEnter(orbis::Thread *thread, int id, uint64_t *args,
                       int argsCount) {
  std::printf("   [%u] sys_%u(", thread->tid, id);

  for (int i = 0; i < argsCount; ++i) {
    if (i != 0) {
      std::printf(", ");
    }

    std::printf("%#lx", args[i]);
  }

  std::printf(")\n");
}

static void onSysExit(orbis::Thread *thread, int id, uint64_t *args,
                      int argsCount, orbis::SysResult result) {
  std::printf("%c: [%u] sys_%u(", result.isError() ? 'E' : 'S', thread->tid,
              id);

  for (int i = 0; i < argsCount; ++i) {
    if (i != 0) {
      std::printf(", ");
    }

    std::printf("%#lx", args[i]);
  }

  std::printf(") -> Status %d, Value %lx:%lx\n", result.value(),
              thread->retval[0], thread->retval[1]);
}

struct KernelEventLogger : public orbis::KernelContext::EventListener {
  void onProcessCreated(orbis::Process *process) override {
    std::printf("process %u was created\n", (unsigned)process->pid);
  }
  void onProcessDeleted(orbis::pid_t pid) override {
    std::printf("process %u was deleted\n", (unsigned)pid);
  }
};

int main() {
  KernelEventLogger eventLogger;
  orbis::KernelContext context;
  context.addEventListener(&eventLogger);
  auto initProc = context.createProcess(1);

  initProc->ops = &procOps;
  initProc->onSysEnter = onSysEnter;
  initProc->onSysExit = onSysExit;

  initProc->state = orbis::ProcessState::NORMAL;
  initProc->parentProcess = initProc;
  initProc->sysent = &orbis::freebsd9_sysvec;

  auto initMainThread = allocateGuestThread(initProc, 1);

  CPU cpu{0};
  Event completeEvent;
  cpu.addTask(initMainThread, [&completeEvent] {
    Registers regs{};
    regs.rax = orbis::kSYS_syscall;
    regs.rdi = orbis::kSYS_exit;
    regs.rsi = 0x64;
    g_guestThread->context = &regs;

    orbis::syscall_entry(g_guestThread);

    completeEvent.fire();
  });

  completeEvent.wait();
  delete initMainThread;
  return 0;
}
