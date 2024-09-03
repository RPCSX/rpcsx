#pragma once

namespace rx {
[[noreturn, gnu::format(printf, 1, 2)]] void die(const char *message, ...);
[[gnu::format(printf, 2, 3)]] void dieIf(bool condition, const char *message,
                                         ...);
} // namespace rx
