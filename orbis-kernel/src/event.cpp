#include "event.hpp"

#include "thread/Process.hpp"
#include <algorithm>

orbis::KNote::~KNote() {
  while (!emitters.empty()) {
    emitters.back()->unsubscribe(this);
  }

  if (linked == nullptr) {
    return;
  }

  if (event.filter == kEvFiltProc) {
    auto proc = static_cast<Process *>(linked);

    std::lock_guard lock(proc->event.mutex);
    proc->event.notes.erase(this);
  }
}

void orbis::EventEmitter::emit(sshort filter, uint fflags, intptr_t data,
                               uintptr_t ident) {
  std::lock_guard lock(mutex);

  for (auto note : notes) {
    if (note->event.filter != filter) {
      continue;
    }
    if (fflags != 0) {
      if ((note->event.fflags & fflags) == 0) {
        continue;
      }

      note->event.fflags = fflags;
    }

    if (ident != std::numeric_limits<uintptr_t>::max() &&
        note->event.ident != ident) {
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

void orbis::EventEmitter::emit(
    sshort filter, void *userData,
    std::optional<intptr_t> (*filterFn)(void *userData, KNote *note)) {
  std::lock_guard lock(mutex);

  for (auto note : notes) {
    if (note->event.filter != filter) {
      continue;
    }

    std::lock_guard lock(note->mutex);

    if (note->triggered) {
      continue;
    }

    if (auto data = filterFn(userData, note)) {
      note->event.data = *data;
      note->triggered = true;
      note->queue->cv.notify_all(note->queue->mtx);
    }
  }
}

void orbis::EventEmitter::subscribe(KNote *note) {
  std::lock_guard lock(mutex);
  notes.insert(note);
  note->emitters.emplace_back(this);
}

void orbis::EventEmitter::unsubscribe(KNote *note) {
  std::lock_guard lock(mutex);
  notes.erase(note);

  auto it = std::ranges::find(note->emitters, this);
  if (it == note->emitters.end()) {
    return;
  }

  std::size_t index = it - note->emitters.begin();
  auto lastEmitter = note->emitters.size() - 1;

  if (index != lastEmitter) {
    std::swap(note->emitters[index], note->emitters[lastEmitter]);
  }

  note->emitters.pop_back();
}