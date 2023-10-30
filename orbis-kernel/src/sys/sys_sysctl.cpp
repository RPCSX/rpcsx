#include "KernelContext.hpp"
#include "sys/sysproto.hpp"
#include "utils/Logs.hpp"

orbis::SysResult orbis::sys___sysctl(Thread *thread, ptr<sint> name,
                                     uint namelen, ptr<void> old,
                                     ptr<size_t> oldlenp, ptr<void> new_,
                                     size_t newlen) {
  enum sysctl_ctl { unspec, kern, vm, vfs, net, debug, hw, machdep, user };

  // machdep.tsc_freq

  enum sysctl_kern {
    usrstack = 33,
    kern_14 = 14,
    kern_37 = 37,

    // FIXME
    smp_cpus = 1000,
    sdk_version,
    sched_cpusetsize,
    proc_ptc,
    cpu_mode,
    rng_pseudo,
    backup_restore_mode,
  };

  enum sysctl_hw {
    pagesize = 7,

    // FIXME
    config = 1000,
  };

  enum sysctl_vm {
    // FIXME
    swap_avail = 1000,
    swap_total,
    kern_heap_size,
  };

  enum sysctl_hw_config { chassis_info };

  enum sysctl_machdep {
    // FIXME
    tsc_freq = 1000,
    liverpool,
  };

  enum sysctl_machdep_liverpool { telemetry = 1000, icc_max };

  // for (unsigned int i = 0; i < namelen; ++i) {
  //   std::fprintf(stderr, "   name[%u] = %u\n", i, name[i]);
  // }

  if (namelen == 3) {
    // 1 - 14 - 41 - debug flags?

    if (name[0] == kern && name[1] == 14 && name[2] == 41) {
      // std::printf("   kern.14.41\n");

      if (*oldlenp != 4 || new_ != nullptr || newlen != 0) {
        return ErrorCode::INVAL;
      }

      *(uint32_t *)old = 0;
      return {};
    }

    if (name[0] == kern && name[1] == 14 && name[2] == 42) {
      // std::printf("   kern.14.42\n");

      if ((oldlenp != nullptr && *oldlenp != 0) || new_ == nullptr ||
          newlen != 4) {
        return ErrorCode::INVAL;
      }

      // set record
      auto record = *(uint32_t *)new_;
      // ORBIS_LOG_WARNING("sys___sysctl: set record", record);
      return {};
    }

    if (name[0] == kern && name[1] == 14 && name[2] == 8) {
      // KERN_PROC_PROC

      struct ProcInfo {
        char data[0x448];
      };

      *oldlenp = 0;
      return {};
    }

    if (name[0] == machdep && name[1] == liverpool && name[2] == telemetry) {
      if (*oldlenp != 8 || new_ != nullptr || newlen != 0) {
        return ErrorCode::INVAL;
      }

      *(uint64_t *)old = 0;
      return {};
    }
    if (name[0] == machdep && name[1] == liverpool && name[2] == icc_max) {
      if (*oldlenp != 4 || new_ != nullptr || newlen != 0) {
        return ErrorCode::INVAL;
      }

      *(uint32_t *)old = 0;
      return {};
    }

    if (name[0] == hw && name[1] == config && name[2] == chassis_info) {
      if (*oldlenp != 8 || new_ != nullptr || newlen != 0) {
        return ErrorCode::INVAL;
      }

      *(uint64_t *)old = 0;
      return {};
    }
  }

  if (namelen == 4) {
    if (name[0] == 1 && name[1] == 14 && name[2] == 35) {
      // sceKernelGetAppInfo
      struct app_info {
        uint32_t appId;
        uint32_t unk0;
        uint32_t unk1;
        uint32_t appType;
        char titleId[10];
        uint16_t unk3;
        slong unk5;
        uint32_t unk6;
        uint16_t unk7;
        char unk_[26];
      };

      // 1 - 14 - 35 - pid
      // std::printf("   kern.14.35.%u\n", name[3]);
      app_info result{
          .appType = 0,
          .unk5 = (thread->tproc->isSystem ? slong(0x80000000'00000000) : 0),
      };

      return uwrite((ptr<app_info>)old, result);
    }

    if (name[0] == 1 && name[1] == 14 && name[2] == 44) {
      // GetLibkernelTextLocation
      if (*oldlenp != 16) {
        return ErrorCode::INVAL;
      }

      auto *dest = (uint64_t *)old;

      for (auto [id, mod] : thread->tproc->modulesMap) {
        if (std::string_view("libkernel") == mod->moduleName) {
          dest[0] = (uint64_t)mod->segments[0].addr;
          dest[1] = mod->segments[0].size;
          return {};
        }
      }
    }
  }

  if (namelen == 2) {
    switch (name[0]) {
    case sysctl_ctl::unspec: {
      switch (name[1]) {
      case 3: {
        std::fprintf(stderr, "   unspec - get name of '%s'\n",
                     std::string((char *)new_, newlen).c_str());
        auto searchName = std::string_view((char *)new_, newlen);
        auto *dest = (std::uint32_t *)old;
        std::uint32_t count = 0;

        if (searchName == "kern.smp.cpus") {
          if (*oldlenp < 2 * sizeof(uint32_t)) {
            std::fprintf(stderr, "   %s error\n", searchName.data());
            return ErrorCode::INVAL;
          }

          dest[count++] = kern;
          dest[count++] = smp_cpus;
        } else if (searchName == "machdep.tsc_freq") {
          if (*oldlenp < 2 * sizeof(uint32_t)) {
            std::fprintf(stderr, "   %s error\n", searchName.data());
            return ErrorCode::INVAL;
          }

          dest[count++] = machdep;
          dest[count++] = tsc_freq;
        } else if (searchName == "kern.sdk_version") {
          if (*oldlenp < 2 * sizeof(uint32_t)) {
            std::fprintf(stderr, "   %s error\n", searchName.data());
            return ErrorCode::INVAL;
          }

          dest[count++] = kern;
          dest[count++] = sdk_version;
        } else if (searchName == "kern.rng_pseudo") {
          if (*oldlenp < 2 * sizeof(uint32_t)) {
            std::fprintf(stderr, "   %s error\n", searchName.data());
            return ErrorCode::INVAL;
          }

          dest[count++] = kern;
          dest[count++] = rng_pseudo;
        } else if (searchName == "kern.sched.cpusetsize") {
          if (*oldlenp < 2 * sizeof(uint32_t)) {
            std::fprintf(stderr, "   %s error\n", searchName.data());
            return ErrorCode::INVAL;
          }

          dest[count++] = kern;
          dest[count++] = sched_cpusetsize;
        } else if (searchName == "kern.proc.ptc") {
          if (*oldlenp < 2 * sizeof(uint32_t)) {
            std::fprintf(stderr, "   %s error\n", searchName.data());
            return ErrorCode::INVAL;
          }

          dest[count++] = kern;
          dest[count++] = proc_ptc;
        } else if (searchName == "kern.cpumode") {
          if (*oldlenp < 2 * sizeof(uint32_t)) {
            std::fprintf(stderr, "   %s error\n", searchName.data());
            return ErrorCode::INVAL;
          }

          dest[count++] = kern;
          dest[count++] = cpu_mode;
        } else if (searchName == "kern.backup_restore_mode") {
          if (*oldlenp < 2 * sizeof(uint32_t)) {
            std::fprintf(stderr, "   %s error\n", searchName.data());
            return ErrorCode::INVAL;
          }

          dest[count++] = kern;
          dest[count++] = backup_restore_mode;
        } else if (searchName == "hw.config.chassis_info") {
          if (*oldlenp < 3 * sizeof(uint32_t)) {
            std::fprintf(stderr, "   %s error\n", searchName.data());
            return ErrorCode::INVAL;
          }

          dest[count++] = hw;
          dest[count++] = config;
          dest[count++] = chassis_info;
        } else if (searchName == "machdep.liverpool.telemetry") {
          if (*oldlenp < 3 * sizeof(uint32_t)) {
            std::fprintf(stderr, "   %s error\n", searchName.data());
            return ErrorCode::INVAL;
          }

          dest[count++] = machdep;
          dest[count++] = liverpool;
          dest[count++] = telemetry;
        } else if (searchName == "machdep.liverpool.icc_max") {
          if (*oldlenp < 3 * sizeof(uint32_t)) {
            std::fprintf(stderr, "   %s error\n", searchName.data());
            return ErrorCode::INVAL;
          }

          dest[count++] = machdep;
          dest[count++] = liverpool;
          dest[count++] = icc_max;
        } else if (searchName == "vm.swap_avail") {
          if (*oldlenp < 2 * sizeof(uint32_t)) {
            std::fprintf(stderr, "   %s error\n", searchName.data());
            return ErrorCode::INVAL;
          }

          dest[count++] = vm;
          dest[count++] = swap_avail;
        } else if (searchName == "vm.kern_heap_size") {
          if (*oldlenp < 2 * sizeof(uint32_t)) {
            std::fprintf(stderr, "   %s error\n", searchName.data());
            return ErrorCode::INVAL;
          }

          dest[count++] = vm;
          dest[count++] = kern_heap_size;
        } else if (searchName == "vm.swap_total") {
          if (*oldlenp < 2 * sizeof(uint32_t)) {
            std::fprintf(stderr, "   %s error\n", searchName.data());
            return ErrorCode::INVAL;
          }

          dest[count++] = vm;
          dest[count++] = swap_total;
        }

        if (count == 0) {
          std::fprintf(stderr, "sys___sysctl:   %s is unknown\n",
                       searchName.data());
          return ErrorCode::SRCH;
        }

        *oldlenp = count * sizeof(uint32_t);
        return {};
      }

      default:
        break;
      }
      std::printf("   unspec_%u\n", name[1]);
      return {};
    }

    case sysctl_ctl::kern:
      switch (name[1]) {
      case sysctl_kern::usrstack: {
        if (*oldlenp != 8 || new_ != nullptr || newlen != 0) {
          return ErrorCode::INVAL;
        }

        std::printf("Reporting stack at %p\n", thread->stackEnd);
        *(ptr<void> *)old = thread->stackEnd;
        return {};
      }

      case sysctl_kern::smp_cpus:
        if (*oldlenp != 4 || new_ != nullptr || newlen != 0) {
          return ErrorCode::INVAL;
        }

        *(uint32_t *)old = 1;
        return {};

      case sysctl_kern::sdk_version: {
        if (*oldlenp != 4 || new_ != nullptr || newlen != 0) {
          return ErrorCode::INVAL;
        }

        auto processParam =
            reinterpret_cast<std::byte *>(thread->tproc->processParam);

        auto sdkVersion = processParam        //
                          + sizeof(uint64_t)  // size
                          + sizeof(uint32_t)  // magic
                          + sizeof(uint32_t); // entryCount

        std::printf("Reporting SDK version %x\n",
                    *reinterpret_cast<uint32_t *>(sdkVersion));
        *(uint32_t *)old = *reinterpret_cast<uint32_t *>(sdkVersion);
        return {};
      }

      case sysctl_kern::sched_cpusetsize:
        if (*oldlenp != 4 || new_ != nullptr || newlen != 0) {
          return ErrorCode::INVAL;
        }

        *(std::uint32_t *)old = 4;
        return {};

      case sysctl_kern::rng_pseudo:
        if (*oldlenp != 0x40 || new_ != nullptr || newlen != 0) {
          return ErrorCode::INVAL;
        }

        std::memset(old, 0, 0x40);
        return {};

      case sysctl_kern::kern_37: {
        struct kern37_value {
          std::uint64_t size;
          std::uint64_t unk[7];
        };

        if (*oldlenp != sizeof(kern37_value) || new_ != nullptr ||
            newlen != 0) {
          return ErrorCode::INVAL;
        }

        auto value = (kern37_value *)old;
        value->size = sizeof(kern37_value);
        return {};
      }

      case sysctl_kern::proc_ptc: {
        if (*oldlenp != 8 || new_ != nullptr || newlen != 0) {
          return ErrorCode::INVAL;
        }

        *(std::uint64_t *)old = 1357;
        return {};
      }

      case sysctl_kern::cpu_mode: {
        if (*oldlenp != 4 || new_ != nullptr || newlen != 0) {
          return ErrorCode::INVAL;
        }

        // 0 - 6 cpu
        // 1 - 7 cpu, low power
        // 5 - 7 cpu, normal
        *(std::uint32_t *)old = 5;
        return {};
      }

      case sysctl_kern::backup_restore_mode:
        if (*oldlenp != 4 || new_ != nullptr || newlen != 0) {
          return ErrorCode::INVAL;
        }

        // 0 - normal
        // 1 - backup
        // 2 - restore
        *(std::uint32_t *)old = 0;
        return {};

      default:
        return ErrorCode::INVAL;
      }
      break;

    case sysctl_ctl::vm:
      switch (name[1]) {
      case sysctl_vm::kern_heap_size:
        if (*oldlenp != 4 || new_ != nullptr || newlen != 0) {
          return ErrorCode::INVAL;
        }

        *(std::uint32_t *)old = (1 << 14) >> 14;
        return {};

      case sysctl_vm::swap_total:
        if (*oldlenp != 8 || new_ != nullptr || newlen != 0) {
          return ErrorCode::INVAL;
        }

        *(std::uint64_t *)old = 0;
        return {};

      case sysctl_vm::swap_avail:
        if (*oldlenp != 4 || new_ != nullptr || newlen != 0) {
          return ErrorCode::INVAL;
        }

        *(std::uint32_t *)old = (1 << 14) >> 14;
        return {};

      default:
        break;
      }
    case sysctl_ctl::vfs:
    case sysctl_ctl::net:
    case sysctl_ctl::debug:
      return ErrorCode::INVAL;

    case sysctl_ctl::hw:
      switch (name[1]) {
      case sysctl_hw::pagesize:
        if (*oldlenp != 4 || new_ != nullptr || newlen != 0) {
          return ErrorCode::INVAL;
        }

        *(uint32_t *)old = 0x4000;
        return {};

      default:
        break;
      }
      break;

    case sysctl_ctl::machdep:
      switch (name[1]) {
      case sysctl_machdep::tsc_freq: {
        if (*oldlenp != 8 || new_ != nullptr || newlen != 0) {
          return ErrorCode::INVAL;
        }

        *(uint64_t *)old = g_context.getTscFreq();
        return {};

      default:
        break;
      }
      }
    case sysctl_ctl::user:
      break;
    }
  }

  std::string concatName;
  for (unsigned int i = 0; i < namelen; ++i) {
    if (i != 0) {
      concatName += '.';
    }

    concatName += std::to_string(name[i]);
  }

  std::size_t oldLen = oldlenp ? *oldlenp : 0;
  ORBIS_LOG_TODO(__FUNCTION__, concatName, oldLen, new_, newlen);
  thread->where();
  return {};
}
