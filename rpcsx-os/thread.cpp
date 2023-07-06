#include "thread.hpp"
#include <asm/prctl.h>
#include <immintrin.h>
#include <link.h>
#include <linux/prctl.h>
#include <sys/prctl.h>
#include <sys/ucontext.h>
#include <ucontext.h>
#include <unistd.h>

thread_local orbis::Thread *rx::thread::g_current = nullptr;

struct LibcInfo {
  std::uint64_t textBegin = ~static_cast<std::uint64_t>(0);
  std::uint64_t textSize = 0;
};

static LibcInfo libcInfo;

void rx::thread::initialize() {
  auto processPhdr = [](struct dl_phdr_info *info, size_t, void *data) {
    auto path = std::string_view(info->dlpi_name);
    auto slashPos = path.rfind('/');
    if (slashPos == std::string_view::npos) {
      return 0;
    }

    auto name = path.substr(slashPos + 1);
    if (name.starts_with("libc.so")) {
      std::printf("%s\n", std::string(name).c_str());
      auto libcInfo = reinterpret_cast<LibcInfo *>(data);

      for (std::size_t i = 0; i < info->dlpi_phnum; ++i) {
        auto &phdr = info->dlpi_phdr[i];

        if (phdr.p_type == PT_LOAD && (phdr.p_flags & PF_X) == PF_X) {
          libcInfo->textBegin =
              std::min(libcInfo->textBegin, phdr.p_vaddr + info->dlpi_addr);
          libcInfo->textSize = std::max(libcInfo->textSize, phdr.p_memsz);
        }
      }

      return 1;
    }

    return 0;
  };

  dl_iterate_phdr(processPhdr, &libcInfo);

  std::printf("libc text %zx-%zx\n", libcInfo.textBegin,
              libcInfo.textBegin + libcInfo.textSize);
}

void rx::thread::deinitialize() {}

void rx::thread::invoke(orbis::Thread *thread) {
  g_current = thread;

  std::uint64_t hostFs = _readfsbase_u64();
  _writegsbase_u64(hostFs);

  if (prctl(PR_SET_SYSCALL_USER_DISPATCH, PR_SYS_DISPATCH_ON,
            libcInfo.textBegin, libcInfo.textSize, nullptr)) {
    perror("prctl failed\n");
    exit(-1);
  }

  _writefsbase_u64(thread->fsBase);
  auto context = reinterpret_cast<ucontext_t *>(thread->context);

  asm volatile("movq $0, %%r8\n"
               "movq $0, %%r9\n"
               "movq $0, %%r11\n"
               "movq $0, %%r12\n"
               "movq $0, %%r13\n"
               "movq $0, %%r14\n"
               "movq %1, %%rsp\n"
               "callq *%0\n"
               :
               : "rm"(context->uc_mcontext.gregs[REG_RIP]),
                 "rm"(context->uc_mcontext.gregs[REG_RSP]),
                 "D"(context->uc_mcontext.gregs[REG_RDI]),
                 "S"(context->uc_mcontext.gregs[REG_RSI]),
                 "d"(context->uc_mcontext.gregs[REG_RDX]),
                 "c"(context->uc_mcontext.gregs[REG_RCX]),
                 "b"(context->uc_mcontext.gregs[REG_RBX])
               : "memory");
  _writefsbase_u64(hostFs);
}
