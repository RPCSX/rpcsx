#pragma once

#include "KernelAllocator.hpp"
#include "utils/Rc.hpp"

namespace orbis {
struct IpmiServer : RcBase {
  kstring name;

  explicit IpmiServer(kstring name) : name(std::move(name)) {}
};

struct IpmiClient : RcBase {
  Ref<IpmiServer> connection;
  kstring name;

  explicit IpmiClient(kstring name) : name(std::move(name)) {}
};
} // namespace orbis
