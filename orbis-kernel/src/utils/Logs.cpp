#include "utils/Logs.hpp"
#include "error/ErrorCode.hpp"
#include <cstdarg>
#include <cstdint>
#include <sstream>
#include <string>
#include <string_view>
#include <unistd.h>
#include <vector>

static void append_hex(std::string &out, std::unsigned_integral auto value) {
  std::ostringstream buf;
  if (value < 10)
    buf << value;
  else if (value >= decltype(value)(UINTMAX_MAX) - 1)
    buf << "-" << std::uintmax_t(decltype(value)(-value));
  else
    buf << "0x" << std::hex << std::uintmax_t(value);
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

template <>
void log_class_string<orbis::ErrorCode>::format(std::string &out,
                                                const void *arg) {
  auto errorCode = get_object(arg);
  switch (errorCode) {
  case ErrorCode::PERM:
    out += "PERM";
    return;
  case ErrorCode::NOENT:
    out += "NOENT";
    return;
  case ErrorCode::SRCH:
    out += "SRCH";
    return;
  case ErrorCode::INTR:
    out += "INTR";
    return;
  case ErrorCode::IO:
    out += "IO";
    return;
  case ErrorCode::NXIO:
    out += "NXIO";
    return;
  case ErrorCode::TOOBIG:
    out += "TOOBIG";
    return;
  case ErrorCode::NOEXEC:
    out += "NOEXEC";
    return;
  case ErrorCode::BADF:
    out += "BADF";
    return;
  case ErrorCode::CHILD:
    out += "CHILD";
    return;
  case ErrorCode::DEADLK:
    out += "DEADLK";
    return;
  case ErrorCode::NOMEM:
    out += "NOMEM";
    return;
  case ErrorCode::ACCES:
    out += "ACCES";
    return;
  case ErrorCode::FAULT:
    out += "FAULT";
    return;
  case ErrorCode::NOTBLK:
    out += "NOTBLK";
    return;
  case ErrorCode::BUSY:
    out += "BUSY";
    return;
  case ErrorCode::EXIST:
    out += "EXIST";
    return;
  case ErrorCode::XDEV:
    out += "XDEV";
    return;
  case ErrorCode::NODEV:
    out += "NODEV";
    return;
  case ErrorCode::NOTDIR:
    out += "NOTDIR";
    return;
  case ErrorCode::ISDIR:
    out += "ISDIR";
    return;
  case ErrorCode::INVAL:
    out += "INVAL";
    return;
  case ErrorCode::NFILE:
    out += "NFILE";
    return;
  case ErrorCode::MFILE:
    out += "MFILE";
    return;
  case ErrorCode::NOTTY:
    out += "NOTTY";
    return;
  case ErrorCode::TXTBSY:
    out += "TXTBSY";
    return;
  case ErrorCode::FBIG:
    out += "FBIG";
    return;
  case ErrorCode::NOSPC:
    out += "NOSPC";
    return;
  case ErrorCode::SPIPE:
    out += "SPIPE";
    return;
  case ErrorCode::ROFS:
    out += "ROFS";
    return;
  case ErrorCode::MLINK:
    out += "MLINK";
    return;
  case ErrorCode::PIPE:
    out += "PIPE";
    return;
  case ErrorCode::DOM:
    out += "DOM";
    return;
  case ErrorCode::RANGE:
    out += "RANGE";
    return;
  case ErrorCode::AGAIN:
    out += "AGAIN";
    return;
  case ErrorCode::INPROGRESS:
    out += "INPROGRESS";
    return;
  case ErrorCode::ALREADY:
    out += "ALREADY";
    return;
  case ErrorCode::NOTSOCK:
    out += "NOTSOCK";
    return;
  case ErrorCode::DESTADDRREQ:
    out += "DESTADDRREQ";
    return;
  case ErrorCode::MSGSIZE:
    out += "MSGSIZE";
    return;
  case ErrorCode::PROTOTYPE:
    out += "PROTOTYPE";
    return;
  case ErrorCode::NOPROTOOPT:
    out += "NOPROTOOPT";
    return;
  case ErrorCode::PROTONOSUPPORT:
    out += "PROTONOSUPPORT";
    return;
  case ErrorCode::SOCKTNOSUPPORT:
    out += "SOCKTNOSUPPORT";
    return;
  case ErrorCode::OPNOTSUPP:
    out += "OPNOTSUPP";
    return;
  case ErrorCode::PFNOSUPPORT:
    out += "PFNOSUPPORT";
    return;
  case ErrorCode::AFNOSUPPORT:
    out += "AFNOSUPPORT";
    return;
  case ErrorCode::ADDRINUSE:
    out += "ADDRINUSE";
    return;
  case ErrorCode::ADDRNOTAVAIL:
    out += "ADDRNOTAVAIL";
    return;
  case ErrorCode::NETDOWN:
    out += "NETDOWN";
    return;
  case ErrorCode::NETUNREACH:
    out += "NETUNREACH";
    return;
  case ErrorCode::NETRESET:
    out += "NETRESET";
    return;
  case ErrorCode::CONNABORTED:
    out += "CONNABORTED";
    return;
  case ErrorCode::CONNRESET:
    out += "CONNRESET";
    return;
  case ErrorCode::NOBUFS:
    out += "NOBUFS";
    return;
  case ErrorCode::ISCONN:
    out += "ISCONN";
    return;
  case ErrorCode::NOTCONN:
    out += "NOTCONN";
    return;
  case ErrorCode::SHUTDOWN:
    out += "SHUTDOWN";
    return;
  case ErrorCode::TOOMANYREFS:
    out += "TOOMANYREFS";
    return;
  case ErrorCode::TIMEDOUT:
    out += "TIMEDOUT";
    return;
  case ErrorCode::CONNREFUSED:
    out += "CONNREFUSED";
    return;
  case ErrorCode::LOOP:
    out += "LOOP";
    return;
  case ErrorCode::NAMETOOLONG:
    out += "NAMETOOLONG";
    return;
  case ErrorCode::HOSTDOWN:
    out += "HOSTDOWN";
    return;
  case ErrorCode::HOSTUNREACH:
    out += "HOSTUNREACH";
    return;
  case ErrorCode::NOTEMPTY:
    out += "NOTEMPTY";
    return;
  case ErrorCode::PROCLIM:
    out += "PROCLIM";
    return;
  case ErrorCode::USERS:
    out += "USERS";
    return;
  case ErrorCode::DQUOT:
    out += "DQUOT";
    return;
  case ErrorCode::STALE:
    out += "STALE";
    return;
  case ErrorCode::REMOTE:
    out += "REMOTE";
    return;
  case ErrorCode::BADRPC:
    out += "BADRPC";
    return;
  case ErrorCode::RPCMISMATCH:
    out += "RPCMISMATCH";
    return;
  case ErrorCode::PROGUNAVAIL:
    out += "PROGUNAVAIL";
    return;
  case ErrorCode::PROGMISMATCH:
    out += "PROGMISMATCH";
    return;
  case ErrorCode::PROCUNAVAIL:
    out += "PROCUNAVAIL";
    return;
  case ErrorCode::NOLCK:
    out += "NOLCK";
    return;
  case ErrorCode::NOSYS:
    out += "NOSYS";
    return;
  case ErrorCode::FTYPE:
    out += "FTYPE";
    return;
  case ErrorCode::AUTH:
    out += "AUTH";
    return;
  case ErrorCode::NEEDAUTH:
    out += "NEEDAUTH";
    return;
  case ErrorCode::IDRM:
    out += "IDRM";
    return;
  case ErrorCode::NOMSG:
    out += "NOMSG";
    return;
  case ErrorCode::OVERFLOW:
    out += "OVERFLOW";
    return;
  case ErrorCode::CANCELED:
    out += "CANCELED";
    return;
  case ErrorCode::ILSEQ:
    out += "ILSEQ";
    return;
  case ErrorCode::NOATTR:
    out += "NOATTR";
    return;
  case ErrorCode::DOOFUS:
    out += "DOOFUS";
    return;
  case ErrorCode::BADMSG:
    out += "BADMSG";
    return;
  case ErrorCode::MULTIHOP:
    out += "MULTIHOP";
    return;
  case ErrorCode::NOLINK:
    out += "NOLINK";
    return;
  case ErrorCode::PROTO:
    out += "PROTO";
    return;
  case ErrorCode::NOTCAPABLE:
    out += "NOTCAPABLE";
    return;
  case ErrorCode::CAPMODE:
    out += "CAPMODE";
    return;
  }

  out += "<unknown " + std::to_string((int)errorCode) + ">";
}

void _orbis_log_print(LogLevel lvl, std::string_view msg,
                      std::string_view names, const log_type_info *sup, ...) {
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
  if (args_count)
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
  if (args_count)
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
    color = "\e[0;36m";
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
