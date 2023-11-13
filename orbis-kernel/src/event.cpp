#include "event.hpp"
#include "thread/Process.hpp"

orbis::KNote::~KNote() {
  if (linked == nullptr) {
    return;
  }

  if (event.filter == kEvFiltProc) {
    auto proc = static_cast<Process *>(linked);

    std::lock_guard lock(proc->event.mutex);
    proc->event.notes.erase(this);
  }
}

void orbis::EventEmitter::emit(uint filter, uint fflags, intptr_t data) {
  std::lock_guard lock(mutex);

  for (auto note : notes) {
    if (note->event.filter != filter) {
      continue;
    }
    if (fflags != 0 && ((note->event.fflags & fflags) == 0)) {
      continue;
    }

    std::lock_guard lock(note->mutex);

    if (note->triggered) {
      continue;
    }

    note->triggered = true;
    note->event.data = data;
    note->queue->cv.notify_all(note->queue->mtx);
  }
}
