#include "sys/sysproto.hpp"

#include "KernelContext.hpp"
#include "thread/Process.hpp"

orbis::SysResult orbis::sys_kenv(Thread *thread, sint what,
                                 ptr<const char> name, ptr<char> value,
                                 sint len) {
  enum action { kenv_get, kenv_set, kenv_unset, kenv_dump };

  const auto &[kenv, _] = thread->tproc->context->getKernelEnv();

  if (what == kenv_dump) {
    int needed = 0;
    int done = 0;

    for (const auto &[key, env_value] : kenv) {
      size_t entry = 0;
      // Entry: size of both full buffers, the '=' and the '\0' at the end
      if (value == nullptr || len == 0) {
        entry = key.size() + 1 + strnlen(env_value, 128) + 1;
      } else {
        char buf[128 * 2 + 2];

        char *_buf = buf;
        std::strncpy(buf, key.data(), key.size());
        _buf += key.size();

        *_buf++ = '=';

        const size_t value_size = strnlen(env_value, 128);
        std::strncpy(_buf, env_value, value_size);
        _buf += value_size;

        *_buf++ = '\0';

        entry = _buf - buf;
        ORBIS_RET_ON_ERROR(uwriteRaw(value + done, buf, entry));
        len -= entry;
        done += entry;
      }
      needed += entry;
    }

    if (done != needed) {
      thread->retval[0] = needed;
    }
    return {};
  }

  char _name_buf[128];
  ORBIS_RET_ON_ERROR(ureadString(_name_buf, sizeof(_name_buf), name));
  const std::string_view _name(_name_buf, strnlen(_name_buf, 128));

  switch (what) {
  case kenv_get: {
    const auto it = kenv.find(_name);
    if (it == kenv.end()) {
      return ErrorCode::NOENT;
    }
    const char *buf = it->second;
    ORBIS_RET_ON_ERROR(uwriteRaw(value, buf, std::min(len, 128)));
    break;
  }
  case kenv_set: {
    if (len < 1) {
      return ErrorCode::INVAL;
    }
    char *_value_buf = kenv[kstring(_name)];
    ORBIS_RET_ON_ERROR(ureadString(_value_buf, 128, value));
    break;
  }
  case kenv_unset: {
    const auto it = kenv.find(_name);
    if (it == kenv.end()) {
      return ErrorCode::NOENT;
    }
    kenv.erase(it);
    break;
  }
  default:
    return ErrorCode::INVAL;
  }

  return {};
}
