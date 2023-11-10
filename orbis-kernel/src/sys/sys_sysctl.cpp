#include "KernelContext.hpp"
#include "sys/sysproto.hpp"
#include "utils/Logs.hpp"

orbis::SysResult orbis::sys___sysctl(Thread *thread, ptr<sint> name,
                                     uint namelen, ptr<void> old,
                                     ptr<size_t> oldlenp, ptr<void> new_,
                                     size_t newlen) {

  enum class ctl : int {
    sysctl = 0,
    kern = 1,
    vm = 2,
    vfs = 3,
    net = 4,
    debug = 5,
    hw = 6,
    machdep = 7,
    user = 8
  };
  enum class ctl_sysctl : int {
    debug = 0,
    name = 1,
    next = 2,
    name2oid = 3,
    oidfmt = 4,
    oiddescr = 5,
    oidlabel = 6,
    nextnoskip = 7
  };

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
    console,
    init_safe_mode
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

    ctl name0 = (ctl)name[0];

    if (name0 == ctl::kern && name[1] == 14 && name[2] == 41) {
      // std::printf("   kern.14.41\n");

      if (*oldlenp != 4 || new_ != nullptr || newlen != 0) {
        return ErrorCode::INVAL;
      }

      *(uint32_t *)old = 0;
      return {};
    }

    if (name0 == ctl::kern && name[1] == 14 && name[2] == 42) {
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

    if (name0 == ctl::kern && name[1] == 14 && name[2] == 8) {
      // KERN_PROC_PROC

      struct ProcInfo {
        char data[0x448];
      };

      *oldlenp = 0;
      return {};
    }

    if (name0 == ctl::machdep && name[1] == liverpool && name[2] == telemetry) {
      if (*oldlenp != 8 || new_ != nullptr || newlen != 0) {
        return ErrorCode::INVAL;
      }

      *(uint64_t *)old = 0;
      return {};
    }
    if (name0 == ctl::machdep && name[1] == liverpool && name[2] == icc_max) {
      if (*oldlenp != 4 || new_ != nullptr || newlen != 0) {
        return ErrorCode::INVAL;
      }

      *(uint32_t *)old = 0;
      return {};
    }

    if (name0 == ctl::hw && name[1] == config && name[2] == chassis_info) {
      if (*oldlenp != 8 || new_ != nullptr || newlen != 0) {
        return ErrorCode::INVAL;
      }

      *(uint64_t *)old = 0;
      return {};
    }
  }

  if (namelen == 4) {
    if (name[0] == 1 && name[1] == 14 && name[2] == 35) {
      // AppInfo get/set

      // 1 - 14 - 35 - pid

      if (old) {
        size_t oldlen;
        if (auto errc = uread(oldlen, oldlenp); errc != ErrorCode{}) {
          return errc;
        }

        if (oldlen < sizeof(AppInfo)) {
          return ErrorCode::INVAL;
        }

        if (auto errc = uwrite((ptr<AppInfo>)old, thread->tproc->appInfo);
            errc != ErrorCode{}) {
          return errc;
        }

        if (auto errc = uwrite(oldlenp, sizeof(AppInfo)); errc != ErrorCode{}) {
          return errc;
        }
      }

      if (new_) {
        if (newlen != sizeof(AppInfo)) {
          return ErrorCode::INVAL;
        }

        auto result = uread(thread->tproc->appInfo, (ptr<AppInfo>)new_);
        if (result == ErrorCode{}) {
          auto &appInfo = thread->tproc->appInfo;
          ORBIS_LOG_ERROR("set AppInfo", appInfo.appId, appInfo.unk0,
                          appInfo.unk1, appInfo.appType, appInfo.titleId,
                          appInfo.unk2, appInfo.unk3, appInfo.unk5,
                          appInfo.unk6, appInfo.unk7, appInfo.unk8);

          // HACK
          appInfo.unk4 = orbis::slong(0x80000000'00000000);
        }

        return result;
      }

      return {};
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
    switch ((ctl)name[0]) {
    case ctl::sysctl: {
      switch ((ctl_sysctl)name[1]) {
      case ctl_sysctl::name2oid: {
        auto searchName = std::string_view((char *)new_, newlen);
        auto *dest = (std::uint32_t *)old;
        std::uint32_t count = 0;

        if (searchName == "kern.smp.cpus") {
          if (*oldlenp < 2 * sizeof(uint32_t)) {
            std::fprintf(stderr, "   %s error\n", searchName.data());
            return ErrorCode::INVAL;
          }

          dest[count++] = (int)ctl::kern;
          dest[count++] = smp_cpus;
        } else if (searchName == "machdep.tsc_freq") {
          if (*oldlenp < 2 * sizeof(uint32_t)) {
            std::fprintf(stderr, "   %s error\n", searchName.data());
            return ErrorCode::INVAL;
          }

          dest[count++] = (int)ctl::machdep;
          dest[count++] = tsc_freq;
        } else if (searchName == "kern.sdk_version") {
          if (*oldlenp < 2 * sizeof(uint32_t)) {
            std::fprintf(stderr, "   %s error\n", searchName.data());
            return ErrorCode::INVAL;
          }

          dest[count++] = (int)ctl::kern;
          dest[count++] = sdk_version;
        } else if (searchName == "kern.rng_pseudo") {
          if (*oldlenp < 2 * sizeof(uint32_t)) {
            std::fprintf(stderr, "   %s error\n", searchName.data());
            return ErrorCode::INVAL;
          }

          dest[count++] = (int)ctl::kern;
          dest[count++] = rng_pseudo;
        } else if (searchName == "kern.sched.cpusetsize") {
          if (*oldlenp < 2 * sizeof(uint32_t)) {
            std::fprintf(stderr, "   %s error\n", searchName.data());
            return ErrorCode::INVAL;
          }

          dest[count++] = (int)ctl::kern;
          dest[count++] = sched_cpusetsize;
        } else if (searchName == "kern.proc.ptc") {
          if (*oldlenp < 2 * sizeof(uint32_t)) {
            std::fprintf(stderr, "   %s error\n", searchName.data());
            return ErrorCode::INVAL;
          }

          dest[count++] = (int)ctl::kern;
          dest[count++] = proc_ptc;
        } else if (searchName == "kern.cpumode") {
          if (*oldlenp < 2 * sizeof(uint32_t)) {
            std::fprintf(stderr, "   %s error\n", searchName.data());
            return ErrorCode::INVAL;
          }

          dest[count++] = (int)ctl::kern;
          dest[count++] = cpu_mode;
        } else if (searchName == "kern.backup_restore_mode") {
          if (*oldlenp < 2 * sizeof(uint32_t)) {
            std::fprintf(stderr, "   %s error\n", searchName.data());
            return ErrorCode::INVAL;
          }

          dest[count++] = (int)ctl::kern;
          dest[count++] = backup_restore_mode;
        } else if (searchName == "kern.console") {
          if (*oldlenp < 2 * sizeof(uint32_t)) {
            std::fprintf(stderr, "   %s error\n", searchName.data());
            return ErrorCode::INVAL;
          }

          dest[count++] = (int)ctl::kern;
          dest[count++] = console;
        } else if (searchName == "kern.init_safe_mode") {
          if (*oldlenp < 2 * sizeof(uint32_t)) {
            std::fprintf(stderr, "   %s error\n", searchName.data());
            return ErrorCode::INVAL;
          }

          dest[count++] = (int)ctl::kern;
          dest[count++] = init_safe_mode;
        } else if (searchName == "hw.config.chassis_info") {
          if (*oldlenp < 3 * sizeof(uint32_t)) {
            std::fprintf(stderr, "   %s error\n", searchName.data());
            return ErrorCode::INVAL;
          }

          dest[count++] = (int)ctl::hw;
          dest[count++] = config;
          dest[count++] = chassis_info;
        } else if (searchName == "machdep.liverpool.telemetry") {
          if (*oldlenp < 3 * sizeof(uint32_t)) {
            std::fprintf(stderr, "   %s error\n", searchName.data());
            return ErrorCode::INVAL;
          }

          dest[count++] = (int)ctl::machdep;
          dest[count++] = liverpool;
          dest[count++] = telemetry;
        } else if (searchName == "machdep.liverpool.icc_max") {
          if (*oldlenp < 3 * sizeof(uint32_t)) {
            std::fprintf(stderr, "   %s error\n", searchName.data());
            return ErrorCode::INVAL;
          }

          dest[count++] = (int)ctl::machdep;
          dest[count++] = liverpool;
          dest[count++] = icc_max;
        } else if (searchName == "vm.swap_avail") {
          if (*oldlenp < 2 * sizeof(uint32_t)) {
            std::fprintf(stderr, "   %s error\n", searchName.data());
            return ErrorCode::INVAL;
          }

          dest[count++] = (int)ctl::vm;
          dest[count++] = swap_avail;
        } else if (searchName == "vm.kern_heap_size") {
          if (*oldlenp < 2 * sizeof(uint32_t)) {
            std::fprintf(stderr, "   %s error\n", searchName.data());
            return ErrorCode::INVAL;
          }

          dest[count++] = (int)ctl::vm;
          dest[count++] = kern_heap_size;
        } else if (searchName == "vm.swap_total") {
          if (*oldlenp < 2 * sizeof(uint32_t)) {
            std::fprintf(stderr, "   %s error\n", searchName.data());
            return ErrorCode::INVAL;
          }

          dest[count++] = (int)ctl::vm;
          dest[count++] = swap_total;
        } else {
          std::fprintf(stderr, "   unspec - get name of '%s'\n",
                       std::string((char *)new_, newlen).c_str());
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

    case ctl::kern:
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

    case ctl::vm:
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
    case ctl::vfs:
    case ctl::net:
    case ctl::debug:
      return ErrorCode::INVAL;

    case ctl::hw:
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

    case ctl::machdep:
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
    case ctl::user:
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
