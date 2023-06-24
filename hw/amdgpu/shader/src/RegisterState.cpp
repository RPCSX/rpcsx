#include "RegisterState.hpp"
#include "util/unreachable.hpp"

amdgpu::shader::Value
amdgpu::shader::RegisterState::getRegister(RegisterId regId) {
  auto offset = regId.getOffset();

  if (regId.isScalar()) {
    switch (offset) {
    case 0 ... 103:
      return sgprs[offset];
    case 106:
      return vccLo;
    case 107:
      return vccHi;
    case 124:
      return m0;
    case 126:
      return execLo;
    case 127:
      return execHi;
    case 253:
      return scc;
    case 254:
      return ldsDirect;
    }

    util::unreachable();
  }

  if (regId.isVector()) {
    return vgprs[offset];
  }

  if (regId.isAttr()) {
    return attrs[offset];
  }

  util::unreachable();
}

void amdgpu::shader::RegisterState::setRegister(RegisterId regId,
                                                   Value value) {
  auto offset = regId.getOffset();

  if (regId.isScalar()) {
    switch (offset) {
    case 0 ... 103: sgprs[offset] = value; return;
    case 106: vccLo = value; return;
    case 107: vccHi = value; return;
    case 124: m0 = value; return;
    case 126: execLo = value; return;
    case 127: execHi = value; return;
    case 253: scc = value; return;
    case 254: ldsDirect = value; return;
    }

    util::unreachable();
  }

  if (regId.isVector()) {
    vgprs[offset] = value;
    return;
  }

  if (regId.isAttr()) {
    attrs[offset] = value;
    return;
  }

  util::unreachable();
}
