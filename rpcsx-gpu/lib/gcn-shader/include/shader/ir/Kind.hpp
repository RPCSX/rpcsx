#pragma once

#include <string>
namespace shader::ir {
enum class Kind {
  Spv,
  Builtin,
  AmdGpu,
  Vop2,
  Sop2,
  Sopk,
  Smrd,
  Vop3,
  Mubuf,
  Mtbuf,
  Mimg,
  Ds,
  Vintrp,
  Exp,
  Vop1,
  Vopc,
  Sop1,
  Sopc,
  Sopp,
  MemSSA,

  Count,
};

namespace spv {
const char *getInstructionName(unsigned id);
}
namespace builtin {
const char *getInstructionName(unsigned id);
}
namespace amdgpu {
const char *getInstructionName(unsigned id);
}
namespace vop2 {
const char *getInstructionName(unsigned id);
}
namespace sop2 {
const char *getInstructionName(unsigned id);
}
namespace sopk {
const char *getInstructionName(unsigned id);
}
namespace smrd {
const char *getInstructionName(unsigned id);
}
namespace vop3 {
const char *getInstructionName(unsigned id);
}
namespace mubuf {
const char *getInstructionName(unsigned id);
}
namespace mtbuf {
const char *getInstructionName(unsigned id);
}
namespace mimg {
const char *getInstructionName(unsigned id);
}
namespace ds {
const char *getInstructionName(unsigned id);
}
namespace vintrp {
const char *getInstructionName(unsigned id);
}
namespace exp {
const char *getInstructionName(unsigned id);
}
namespace vop1 {
const char *getInstructionName(unsigned id);
}
namespace vopc {
const char *getInstructionName(unsigned id);
}
namespace sop1 {
const char *getInstructionName(unsigned id);
}
namespace sopc {
const char *getInstructionName(unsigned id);
}
namespace sopp {
const char *getInstructionName(unsigned id);
}

namespace memssa {
const char *getInstructionName(unsigned id);
}

inline const char *getKindName(Kind kind) {
  switch (kind) {
  case Kind::Spv:
    return "spv";
  case Kind::Builtin:
    return "builtin";
  case Kind::AmdGpu:
    return "amdgpu";
  case Kind::Vop2:
    return "vop2";
  case Kind::Sop2:
    return "sop2";
  case Kind::Sopk:
    return "sopk";
  case Kind::Smrd:
    return "smrd";
  case Kind::Vop3:
    return "vop3";
  case Kind::Mubuf:
    return "mubuf";
  case Kind::Mtbuf:
    return "mtbuf";
  case Kind::Mimg:
    return "mimg";
  case Kind::Ds:
    return "ds";
  case Kind::Vintrp:
    return "vintrp";
  case Kind::Exp:
    return "exp";
  case Kind::Vop1:
    return "vop1";
  case Kind::Vopc:
    return "vopc";
  case Kind::Sop1:
    return "sop1";
  case Kind::Sopc:
    return "sopc";
  case Kind::Sopp:
    return "sopp";
  case Kind::MemSSA:
    return "memssa";

  case Kind::Count:
    break;
  }

  return "<invalid>";
}
inline const char *getInstructionShortName(Kind kind, unsigned op) {
  switch (kind) {
  case Kind::Spv:
    return spv::getInstructionName(op);
  case Kind::Builtin:
    return builtin::getInstructionName(op);
  case Kind::AmdGpu:
    return amdgpu::getInstructionName(op);
  case Kind::Vop2:
    return vop2::getInstructionName(op);
  case Kind::Sop2:
    return sop2::getInstructionName(op);
  case Kind::Sopk:
    return sopk::getInstructionName(op);
  case Kind::Smrd:
    return smrd::getInstructionName(op);
  case Kind::Vop3:
    return vop3::getInstructionName(op);
  case Kind::Mubuf:
    return mubuf::getInstructionName(op);
  case Kind::Mtbuf:
    return mtbuf::getInstructionName(op);
  case Kind::Mimg:
    return mimg::getInstructionName(op);
  case Kind::Ds:
    return ds::getInstructionName(op);
  case Kind::Vintrp:
    return vintrp::getInstructionName(op);
  case Kind::Exp:
    return exp::getInstructionName(op);
  case Kind::Vop1:
    return vop1::getInstructionName(op);
  case Kind::Vopc:
    return vopc::getInstructionName(op);
  case Kind::Sop1:
    return sop1::getInstructionName(op);
  case Kind::Sopc:
    return sopc::getInstructionName(op);
  case Kind::Sopp:
    return sopp::getInstructionName(op);
  case Kind::MemSSA:
    return memssa::getInstructionName(op);

  case Kind::Count:
    break;
  }

  return nullptr;
}

inline std::string getInstructionName(Kind kind, unsigned op) {
  std::string result = getKindName(kind);
  result += '.';

  if (auto name = getInstructionShortName(kind, op)) {
    result += name;
  } else {
    result += "<invalid ";
    result += std::to_string(op);
    result += ">";
  }

  return result;
}
} // namespace ir
