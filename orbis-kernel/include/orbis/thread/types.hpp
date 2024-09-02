#pragma once
#include "orbis-config.hpp"

namespace orbis {
using lwpid_t = int32_t;
using pid_t = int64_t;
using uid_t = uint32_t;
using gid_t = uint32_t;

struct rtprio {
  uint16_t type;
  uint16_t prio;
};

struct thr_param {
  ptr<void(void *)> start_func;
  ptr<void> arg;
  ptr<char> stack_base;
  size_t stack_size;
  ptr<char> tls_base;
  size_t tls_size;
  ptr<slong> child_tid;  // Address to store the new thread identifier, for the
                         // child's use
  ptr<slong> parent_tid; // Address to store the new thread identifier, for the
                         // parent's use
  sint flags;      // Thread	creation flags. The flags member may specify the
                   // following flags:
  ptr<rtprio> rtp; // Real-time scheduling priority for the new thread. May be
                   // NULL to inherit the priority from the creating	thread
  ptr<char> name;
  ptr<void> spare[2];
};

static constexpr auto RFFDG = 1 << 2;    // copy fd table
static constexpr auto RFPROC = 1 << 4;   // change child (else changes curproc)
static constexpr auto RFMEM = 1 << 5;    // share `address space'
static constexpr auto RFNOWAIT = 1 << 6; // give child to init
static constexpr auto RFCFDG = 1 << 12;  // close all fds, zero fd table
static constexpr auto RFSIGSHARE = 1 << 14; // share signal handlers
static constexpr auto RFLINUXTHPN =
    1 << 16; // do linux clone exit parent notification
static constexpr auto RFTSIGZMB =
    1 << 19; // select signal for exit parent notification
static constexpr auto RFTSIGSHIFT =
    20; // selected signal number is in bits 20-27
static constexpr auto RFTSIGMASK = 0xFF;
static constexpr auto RFPROCDESC = 1 << 28; // return a process descriptor
static constexpr auto RFPPWAIT =
    1 << 31; // parent sleeps until child exits (vfork)

} // namespace orbis
