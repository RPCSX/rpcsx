#pragma once

namespace shader::ir::mtbuf {
enum Op {
  LOAD_FORMAT_X,
  LOAD_FORMAT_XY,
  LOAD_FORMAT_XYZ,
  LOAD_FORMAT_XYZW,
  STORE_FORMAT_X,
  STORE_FORMAT_XY,
  STORE_FORMAT_XYZ,
  STORE_FORMAT_XYZW,

  OpCount
};
inline const char *getInstructionName(unsigned id) {
  switch (id) {
  case LOAD_FORMAT_X:
    return "tbuffer_load_format_x";
  case LOAD_FORMAT_XY:
    return "tbuffer_load_format_xy";
  case LOAD_FORMAT_XYZ:
    return "tbuffer_load_format_xyz";
  case LOAD_FORMAT_XYZW:
    return "tbuffer_load_format_xyzw";
  case STORE_FORMAT_X:
    return "tbuffer_store_format_x";
  case STORE_FORMAT_XY:
    return "tbuffer_store_format_xy";
  case STORE_FORMAT_XYZ:
    return "tbuffer_store_format_xyz";
  case STORE_FORMAT_XYZW:
    return "tbuffer_store_format_xyzw";
  }
  return nullptr;
}
} // namespace shader::ir::mtbuf
