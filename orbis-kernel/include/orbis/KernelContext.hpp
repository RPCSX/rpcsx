#pragma once
#include "orbis/thread/Process.hpp"
#include "utils/LinkedNode.hpp"
#include "utils/SharedMutex.hpp"

#include <algorithm>
#include <mutex>
#include <utility>
#include <vector>

namespace orbis {
class KernelContext {
public:
  struct EventListener {
    virtual ~EventListener() = default;
    virtual void onProcessCreated(Process *) = 0;
    virtual void onProcessDeleted(pid_t pid) = 0;
  };

  ~KernelContext() {
    while (m_processes != nullptr) {
      deleteProcess(&m_processes->object);
    }
  }

  void addEventListener(EventListener *listener) {
    m_event_listeners.push_back(listener);
  }

  void removeEventListener(EventListener *listener) {
    auto it =
        std::find(m_event_listeners.begin(), m_event_listeners.end(), listener);

    if (it != m_event_listeners.end()) {
      m_event_listeners.erase(it);
    }
  }

  Process *createProcess(pid_t pid) {
    auto newProcess = new utils::LinkedNode<Process>();
    newProcess->object.context = this;
    newProcess->object.pid = pid;
    newProcess->object.state = ProcessState::NEW;

    {
      std::lock_guard lock(m_proc_mtx);
      if (m_processes != nullptr) {
        m_processes->insertPrev(*newProcess);
      }

      m_processes = newProcess;
    }

    for (auto listener : m_event_listeners) {
      listener->onProcessCreated(&newProcess->object);
    }

    return &newProcess->object;
  }

  void deleteProcess(Process *proc) {
    auto procNode = reinterpret_cast<utils::LinkedNode<Process> *>(proc);
    auto pid = proc->pid;

    {
      std::lock_guard lock(m_proc_mtx);
      auto next = procNode->erase();

      if (procNode == m_processes) {
        m_processes = next;
      }
    }

    delete procNode;

    for (auto listener : m_event_listeners) {
      listener->onProcessDeleted(pid);
    }
  }

  Process *findProcessById(pid_t pid) const {
    std::lock_guard lock(m_proc_mtx);
    for (auto proc = m_processes; proc != nullptr; proc = proc->next) {
      if (proc->object.pid == pid) {
        return &proc->object;
      }
    }

    return nullptr;
  }

private:
  mutable shared_mutex m_proc_mtx;
  utils::LinkedNode<Process> *m_processes = nullptr;
  std::vector<EventListener *> m_event_listeners;
};
} // namespace orbis