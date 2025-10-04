#include "pipe.hpp"
#include "error/ErrorCode.hpp"
#include "file.hpp"
#include "thread/Thread.hpp"
#include "uio.hpp"
#include "utils/Logs.hpp"
#include <span>

static orbis::ErrorCode pipe_read(orbis::File *file, orbis::Uio *uio,
                                  orbis::Thread *thread) {
  auto pipe = static_cast<orbis::Pipe *>(file);
  while (true) {
    if (pipe->data.empty()) {
      // pipe->cv.wait(file->mtx);
      // ORBIS_LOG_ERROR(__FUNCTION__, "wakeup", thread->name, thread->tid,
      // file); continue;
      return orbis::ErrorCode::WOULDBLOCK;
    }

    for (auto vec : std::span(uio->iov, uio->iovcnt)) {
      auto size = std::min<std::size_t>(pipe->data.size(), vec.len);

      if (size == 0) {
        pipe->data.clear();
        continue;
      }

      if (size > pipe->data.size()) {
        size = pipe->data.size();
      }

      uio->offset += size;
      std::memcpy(vec.base, pipe->data.data(), size);

      ORBIS_LOG_ERROR(__FUNCTION__, thread->name, thread->tid, file, size,
                      pipe->data.size(), uio->offset, file->nextOff);

      if (pipe->data.size() == size) {
        pipe->data.clear();
        break;
      }

      std::memmove(pipe->data.data(), pipe->data.data() + size,
                   pipe->data.size() - size);
      pipe->data.resize(pipe->data.size() - size);
    }

    break;
  }

  pipe->event->emit(orbis::kEvFiltWrite);
  return {};
}

static orbis::ErrorCode pipe_write(orbis::File *file, orbis::Uio *uio,
                                   orbis::Thread *thread) {
  auto pipe = static_cast<orbis::Pipe *>(file)->other;
  ORBIS_LOG_ERROR(__FUNCTION__, thread->name, thread->tid, file);

  std::size_t cnt = 0;
  for (auto vec : std::span(uio->iov, uio->iovcnt)) {
    auto offset = pipe->data.size();
    pipe->data.resize(offset + vec.len);
    ORBIS_RET_ON_ERROR(orbis::ureadRaw(pipe->data.data(), vec.base, vec.len));
    cnt += vec.len;
  }

  pipe->event->emit(orbis::kEvFiltRead);
  pipe->cv.notify_one(file->mtx);
  uio->resid -= cnt;
  uio->offset += cnt;

  ORBIS_LOG_ERROR(__FUNCTION__, thread->name, thread->tid, file, uio->resid,
                  uio->offset, file->nextOff, cnt);
  thread->where();
  return {};
}

static orbis::FileOps pipe_ops = {
    .read = pipe_read,
    .write = pipe_write,
};

std::pair<rx::Ref<orbis::Pipe>, rx::Ref<orbis::Pipe>> orbis::createPipe() {
  auto a = knew<Pipe>();
  auto b = knew<Pipe>();
  a->event = knew<EventEmitter>();
  b->event = knew<EventEmitter>();
  a->ops = &pipe_ops;
  b->ops = &pipe_ops;
  a->other = b;
  b->other = a;
  return {a, b};
}
