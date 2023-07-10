#include "utils/Logs.hpp"
#include <cstdarg>
#include <cstdint>
#include <sstream>
#include <string>
#include <string_view>
#include <unistd.h>
#include <vector>

static void append_hex(std::string &out, std::uintmax_t value) {
  std::ostringstream buf;
  buf << "0x" << std::hex << value;
  out += buf.str();
}

namespace orbis::logs {
void log_class_string<void *>::format(std::string &out, const void *arg) {
  const void *ptr = *reinterpret_cast<const void *const *>(arg);
  append_hex(out, reinterpret_cast<std::uintptr_t>(ptr));
}

void log_class_string<char *>::format_n(std::string &out, const void *arg,
                                        std::size_t n) {
  const char *ptr = reinterpret_cast<const char *>(arg);
  const auto addr = reinterpret_cast<std::uintptr_t>(ptr);
  const auto _end = n ? addr + n - 1 : addr;
  if (addr < 0x10000 || std::max(n, _end) > 0x7fff'ffff'ffff) {
    out += "{{{{{BAD_ADDR:";
    append_hex(out, addr);
    out += "}}}}}";
    return;
  }
  while (n--) {
    const char c = *ptr++;
    if (!c)
      break;
    out += c;
  }
}

void log_class_string<char *>::format(std::string &out, const void *arg) {
  const char *ptr = *reinterpret_cast<const char *const *>(arg);
  const auto addr = reinterpret_cast<std::uintptr_t>(ptr);
  if (addr < 0x10000 || addr > 0x7fff'ffff'ffff) {
    out += "{{{{{BAD_ADDR:";
    append_hex(out, addr);
    out += "}}}}}";
    return;
  }
  out += ptr;
}

template <>
void log_class_string<std::string>::format(std::string &out, const void *arg) {
  out += get_object(arg);
}

template <>
void log_class_string<std::string_view>::format(std::string &out,
                                                const void *arg) {
  out += get_object(arg);
}

template <>
void log_class_string<std::vector<char>>::format(std::string &out,
                                                 const void *arg) {
  const std::vector<char> &obj = get_object(arg);
  out.append(obj.cbegin(), obj.cend());
}

template <>
void log_class_string<std::u8string>::format(std::string &out,
                                             const void *arg) {
  const std::u8string &obj = get_object(arg);
  out.append(obj.cbegin(), obj.cend());
}

template <>
void log_class_string<std::u8string_view>::format(std::string &out,
                                                  const void *arg) {
  const std::u8string_view &obj = get_object(arg);
  out.append(obj.cbegin(), obj.cend());
}

template <>
void log_class_string<std::vector<char8_t>>::format(std::string &out,
                                                    const void *arg) {
  const std::vector<char8_t> &obj = get_object(arg);
  out.append(obj.cbegin(), obj.cend());
}

template <>
void log_class_string<char>::format(std::string &out, const void *arg) {
  append_hex(out, static_cast<unsigned char>(get_object(arg)));
}

template <>
void log_class_string<unsigned char>::format(std::string &out,
                                             const void *arg) {
  append_hex(out, get_object(arg));
}

template <>
void log_class_string<signed char>::format(std::string &out, const void *arg) {
  append_hex(out, static_cast<unsigned char>(get_object(arg)));
}

template <>
void log_class_string<short>::format(std::string &out, const void *arg) {
  append_hex(out, static_cast<unsigned short>(get_object(arg)));
}

template <>
void log_class_string<ushort>::format(std::string &out, const void *arg) {
  append_hex(out, get_object(arg));
}

template <>
void log_class_string<int>::format(std::string &out, const void *arg) {
  append_hex(out, static_cast<uint>(get_object(arg)));
}

template <>
void log_class_string<uint>::format(std::string &out, const void *arg) {
  append_hex(out, get_object(arg));
}

template <>
void log_class_string<long>::format(std::string &out, const void *arg) {
  append_hex(out, static_cast<unsigned long>(get_object(arg)));
}

template <>
void log_class_string<ulong>::format(std::string &out, const void *arg) {
  append_hex(out, get_object(arg));
}

template <>
void log_class_string<long long>::format(std::string &out, const void *arg) {
  append_hex(out, static_cast<unsigned long long>(get_object(arg)));
}

template <>
void log_class_string<unsigned long long>::format(std::string &out,
                                                  const void *arg) {
  append_hex(out, get_object(arg));
}

template <>
void log_class_string<float>::format(std::string &out, const void *arg) {
  std::ostringstream buf(out, std::ios_base::ate);
  buf << get_object(arg);
}

template <>
void log_class_string<double>::format(std::string &out, const void *arg) {
  std::ostringstream buf(out, std::ios_base::ate);
  buf << get_object(arg);
}

template <>
void log_class_string<bool>::format(std::string &out, const void *arg) {
  out += get_object(arg) ? "1" : "0";
}

void _orbis_log_print(LogLevel lvl, const char *msg, std::string_view names,
                      const log_type_info *sup, ...) {
  if (lvl > logs_level.load(std::memory_order::relaxed)) {
    return;
  }

  /*constinit thread_local*/ std::string text;
  /*constinit thread_local*/ std::vector<const void *> args;

  std::size_t args_count = 0;
  for (auto v = sup; v && v->log_string; v++)
    args_count++;

  text.reserve(50000);
  args.resize(args_count);

  va_list c_args;
  va_start(c_args, sup);
  for (const void *&arg : args)
    arg = va_arg(c_args, const void *);
  va_end(c_args);

  text += msg;
  text += "(";
  for (std::size_t i = 0; i < args_count; i++) {
    if (i)
      text += ", ";
    names.remove_prefix(names.find_first_not_of(" \t\n\r"));
    std::string_view name = names.substr(0, names.find_first_of(","));
    names.remove_prefix(name.size() + 1);
    text += name;
    text += "=";
    sup[i].log_string(text, args[i]);
  }
  text += ")";

  const char *color = "";
  switch (lvl) {
  case LogLevel::Always:
    color = "\e[36;1m";
    break;
  case LogLevel::Fatal:
    color = "\e[35;1m";
    break;
  case LogLevel::Error:
    color = "\e[0;31m";
    break;
  case LogLevel::Todo:
    color = "\e[1;33m";
    break;
  case LogLevel::Success:
    color = "\e[1;32m";
    break;
  case LogLevel::Warning:
    color = "\e[0;33m";
    break;
  case LogLevel::Notice:
    color = "";
    break;
  case LogLevel::Trace:
    color = "";
    break;
  }

  static const bool istty = isatty(fileno(stderr));
  if (istty) {
    std::fprintf(stderr, "%s%s\e[0m\n", color, text.c_str());
  } else {
    std::fprintf(stderr, "%s\n", text.c_str());
  }
}
} // namespace orbis::logs
