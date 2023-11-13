#include "pipe.hpp"
#include "error/ErrorCode.hpp"
#include "file.hpp"
#include "uio.hpp"
#include <span>
#include <thread>

static orbis::ErrorCode pipe_read(orbis::File *file, orbis::Uio *uio,
                                  orbis::Thread *thread) {
  auto pipe = static_cast<orbis::Pipe *>(file);
  while (true) {
    if (pipe->data.empty()) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    if (pipe->data.empty()) {
      continue;
    }

    for (auto vec : std::span(uio->iov, uio->iovcnt)) {
      auto size = std::min<std::size_t>(pipe->data.size(), vec.len);
      uio->offset += size;
      std::memcpy(vec.base, pipe->data.data(), size);

      if (pipe->data.size() == size) {
        break;
      }

      std::memmove(pipe->data.data(), pipe->data.data() + size,
                   pipe->data.size() - size);
      pipe->data.resize(pipe->data.size() - size);
    }

    break;
  }
  return {};
}

static orbis::ErrorCode pipe_write(orbis::File *file, orbis::Uio *uio,
                                   orbis::Thread *thread) {
  auto pipe = static_cast<orbis::Pipe *>(file);

  for (auto vec : std::span(uio->iov, uio->iovcnt)) {
    auto offset = pipe->data.size();
    pipe->data.resize(offset + vec.len);
    std::memcpy(pipe->data.data(), vec.base, vec.len);
  }
  uio->resid = 0;
  return {};
}

static orbis::FileOps pipe_ops = {.read = pipe_read, .write = pipe_write};

orbis::Ref<orbis::Pipe> orbis::createPipe() {
  auto result = knew<Pipe>();
  result->ops = &pipe_ops;
  return result;
}
