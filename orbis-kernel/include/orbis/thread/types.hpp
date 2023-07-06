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
	ptr<slong> child_tid; // Address to store the new thread identifier, for the child's use
	ptr<slong> parent_tid; // Address to store the new thread identifier, for the parent's use
	sint flags; // Thread	creation flags. The flags member may specify the following flags:
	ptr<rtprio> rtp; // Real-time scheduling priority for the new thread. May be NULL to inherit the priority from the creating	thread
};
} // namespace orbis
