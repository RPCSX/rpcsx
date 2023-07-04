#include "sys/sysproto.hpp"

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

  };

  enum sysctl_hw {
    pagesize = 7,
  };

  enum sysctl_machdep {
    // FIXME
    tsc_freq = 1000
  };

  // for (unsigned int i = 0; i < namelen; ++i) {
  //   std::printf("   name[%u] = %u\n", i, name[i]);
  // }

  if (namelen == 3) {
    // 1 - 14 - 41 - debug flags?

    if (name[0] == 1 && name[1] == 14 && name[2] == 41) {
      // std::printf("   kern.14.41\n");

      if (*oldlenp != 4 || new_ != nullptr || newlen != 0) {
        return ErrorCode::INVAL;
      }

      *(uint32_t *)old = 0;
      return {};
    }
  }

  if (namelen == 4) {
    // 1 - 14 - 35 - 2

    // sceKernelGetAppInfo
    struct app_info {
      char unk[72];
    };

    if (name[0] == 1 && name[1] == 14 && name[2] == 35) {
      // std::printf("   kern.14.35.%u\n", name[3]);
      memset(old, 0, sizeof(app_info));
      return {};
    }
  }

  if (namelen == 2) {
    switch (name[0]) {
    case sysctl_ctl::unspec: {
      switch (name[1]) {
      case 3: {
        std::printf("   unspec - get name of '%s'\n",
                    std::string((char *)new_, newlen).c_str());
        auto searchName = std::string_view((char *)new_, newlen);
        auto *dest = (std::uint32_t *)old;
        std::uint32_t count = 0;

        if (searchName == "kern.smp.cpus") {
          if (*oldlenp < 2 * sizeof(uint32_t)) {
            return ErrorCode::INVAL;
          }

          dest[count++] = kern;
          dest[count++] = smp_cpus;
        } else if (searchName == "machdep.tsc_freq") {
          if (*oldlenp < 2 * sizeof(uint32_t)) {
            return ErrorCode::INVAL;
          }

          dest[count++] = machdep;
          dest[count++] = tsc_freq;
        } else if (searchName == "kern.sdk_version") {
          if (*oldlenp < 2 * sizeof(uint32_t)) {
            return ErrorCode::INVAL;
          }

          dest[count++] = kern;
          dest[count++] = sdk_version;
        } else if (searchName == "kern.sched.cpusetsize") {
          if (*oldlenp < 2 * sizeof(uint32_t)) {
            return ErrorCode::INVAL;
          }

          dest[count++] = kern;
          dest[count++] = sched_cpusetsize;
        } else if (searchName == "kern.proc.ptc") {
          if (*oldlenp < 2 * sizeof(uint32_t)) {
            std::printf("   kern.proc.ptc error\n");
            return ErrorCode::INVAL;
          }

          dest[count++] = kern;
          dest[count++] = proc_ptc;
        }

        if (count == 0) {
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
        break;

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
        break;
      }

      case sysctl_kern::sched_cpusetsize:
        if (*oldlenp != 4 || new_ != nullptr || newlen != 0) {
          return ErrorCode::INVAL;
        }

        *(std::uint32_t *)old = 4;
        break;

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
        break;
      }

      case sysctl_kern::proc_ptc: {
        if (*oldlenp != 8 || new_ != nullptr || newlen != 0) {
          return ErrorCode::INVAL;
        }

        *(std::uint64_t *)old = 1357;
      }

      default:
        return ErrorCode::INVAL;
      }
      break;

    case sysctl_ctl::vm:
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
        break;

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

        *(uint64_t *)old = 1000000000ull;
        return {};

      default:
        return ErrorCode::INVAL;
      }
      }
    case sysctl_ctl::user:
      return ErrorCode::INVAL;
    }
  }

  return {};
}
