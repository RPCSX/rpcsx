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
