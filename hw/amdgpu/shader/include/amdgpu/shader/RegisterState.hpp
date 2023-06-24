#pragma once
#include "RegisterId.hpp"
#include "Value.hpp"
#include <cstdint>

namespace amdgpu::shader {
struct RegisterState {
  std::uint64_t pc;

  Value sgprs[104];
  Value vccLo;
  Value vccHi;
  Value m0;
  Value execLo;
  Value execHi;
  Value scc;
  Value ldsDirect;
  Value vgprs[512];
  Value attrs[32];

  Value getRegister(RegisterId regId);
  void setRegister(RegisterId regId, Value value);

private:
  Value getRegisterImpl(RegisterId regId);
};
} // namespace amdgpu::shader
