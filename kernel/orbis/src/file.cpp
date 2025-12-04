#include "file.hpp"

std::string orbis::File::toString() {
  if (ops && ops->toString) {
    return ops->toString(this);
  }

  if (device) {
    return device->toString();
  }

  return "<null device>";
}
