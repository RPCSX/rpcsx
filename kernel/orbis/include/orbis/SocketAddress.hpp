#pragma once

namespace orbis {
struct SocketAddress {
  unsigned char len;
  unsigned char family;
  char data[14];
};
} // namespace orbis