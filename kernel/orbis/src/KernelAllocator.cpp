#include "KernelAllocator.hpp"
#include "KernelObject.hpp"
#include "rx/Mappable.hpp"
#include "rx/Serializer.hpp"
#include "rx/SharedMutex.hpp"
#include "rx/die.hpp"
#include "rx/mem.hpp"
#include "rx/print.hpp"

static const std::uint64_t g_allocProtWord = 0xDEADBEAFBADCAFE1;
static constexpr std::uintptr_t kHeapBaseAddress = 0x7100'0000'0000;
static constexpr auto kHeapSize = 0x1'0000'0000;
static constexpr int kDebugHeap = 0;

static constexpr auto kHeapRange =
    rx::AddressRange::fromBeginSize(kHeapBaseAddress, kHeapSize);

namespace orbis {
struct KernelMemoryResource {
  mutable rx::shared_mutex m_heap_mtx;
  rx::shared_mutex m_heap_map_mtx;
  void *m_heap_next = nullptr;
  rx::Mappable m_heap;

  kmultimap<std::size_t, void *> m_free_heap;
  kmultimap<std::size_t, void *> m_used_node;

  ~KernelMemoryResource() { rx::mem::release(kHeapRange, rx::mem::pageSize); }

  void *kalloc(std::size_t size,
               std::size_t align = __STDCPP_DEFAULT_NEW_ALIGNMENT__);
  void kfree(void *ptr, std::size_t size);

  void serialize(rx::Serializer &) const {
    // FIXME: implement
  }
  void deserialize(rx::Deserializer &) {
    // FIXME: implement
  }

  void lock() const { m_heap_mtx.lock(); }
  void unlock() const { m_heap_mtx.unlock(); }
};

static KernelMemoryResource *sMemoryResource;
std::byte *g_globalStorage;

using GlobalStorage =
    kernel::StaticKernelObjectStorage<OrbisNamespace,
                                      kernel::detail::GlobalScope>;

void initializeAllocator() {
  {
    auto errc = rx::mem::reserve(kHeapRange);

    rx::dieIf(errc != std::errc{},
              "kernel heap reservation failed: {:x}-{:x}, errno {}",
              kHeapBaseAddress, kHeapBaseAddress + kHeapSize,
              static_cast<int>(errc));
  }

  auto [heap, errc] = rx::Mappable::CreateMemory(kHeapSize);

  rx::dieIf(errc != std::errc{}, "kernel heap allocation failed: errno {}",
            static_cast<int>(errc));

  errc =
      heap.map(kHeapRange, 0, rx::mem::Protection::R | rx::mem::Protection::W,
               rx::mem::pageSize);

  rx::dieIf(errc != std::errc{}, "kernel heap map failed: errno {}",
            static_cast<int>(errc));

  auto ptr = reinterpret_cast<std::byte *>(kHeapRange.beginAddress());
  sMemoryResource = new (ptr) KernelMemoryResource();
  sMemoryResource->m_heap_next = ptr + sizeof(KernelMemoryResource);
  sMemoryResource->m_heap = std::move(heap);

  rx::print(stderr, "global: size {}, alignment {}\n", GlobalStorage::GetSize(),
            GlobalStorage::GetAlignment());
  // allocate whole global storage
  g_globalStorage = (std::byte *)sMemoryResource->kalloc(
      GlobalStorage::GetSize(), GlobalStorage::GetAlignment());
}

void deinitializeAllocator() {
  sMemoryResource->kfree(g_globalStorage, GlobalStorage::GetSize());
  delete sMemoryResource;
  sMemoryResource = nullptr;
  g_globalStorage = nullptr;
}

void *KernelMemoryResource::kalloc(std::size_t size, std::size_t align) {
  size = (size + (__STDCPP_DEFAULT_NEW_ALIGNMENT__ - 1)) &
         ~(__STDCPP_DEFAULT_NEW_ALIGNMENT__ - 1);
  rx::dieIf(size == 0, "kalloc: zero size");

  if (m_heap_map_mtx.try_lock()) {
    std::lock_guard lock(m_heap_map_mtx, std::adopt_lock);

    // Try to reuse previously freed block
    for (auto [it, end] = m_free_heap.equal_range(size); it != end; ++it) {
      auto result = it->second;
      if (!(std::bit_cast<std::uintptr_t>(result) & (align - 1))) {
        auto node = m_free_heap.extract(it);
        node.key() = 0;
        node.mapped() = nullptr;
        m_used_node.insert(m_used_node.begin(), std::move(node));

        // std::fprintf(stderr, "kalloc: reuse %p-%p, size = %lx\n", result,
        //              (char *)result + size, size);

        if (kDebugHeap > 0) {
          std::memcpy(std::bit_cast<std::byte *>(result) + size,
                      &g_allocProtWord, sizeof(g_allocProtWord));
        }
        return result;
      }
    }
  }

  std::lock_guard lock(m_heap_mtx);
  align = std::max<std::size_t>(align, __STDCPP_DEFAULT_NEW_ALIGNMENT__);
  auto heap = reinterpret_cast<std::uintptr_t>(m_heap_next);
  heap = (heap + (align - 1)) & ~(align - 1);

  if (kDebugHeap > 1) {
    if (auto diff =
            (heap + size + sizeof(g_allocProtWord)) & (rx::mem::pageSize - 1);
        diff != 0) {
      heap += rx::mem::pageSize - diff;
      heap &= ~(align - 1);
    }
  }

  rx::dieIf(heap + size > kHeapBaseAddress + kHeapSize,
            "kalloc: out of kernel memory");

  // Check overflow
  rx::dieIf(heap + size < heap, "kalloc: too big allocation");

  // std::fprintf(stderr, "kalloc: allocate %lx-%lx, size = %lx, align=%lx\n",
  //              heap, heap + size, size, align);

  auto result = reinterpret_cast<void *>(heap);
  if (kDebugHeap > 0) {
    std::memcpy(std::bit_cast<std::byte *>(result) + size, &g_allocProtWord,
                sizeof(g_allocProtWord));
  }

  if (kDebugHeap > 0) {
    m_heap_next =
        reinterpret_cast<void *>(heap + size + sizeof(g_allocProtWord));
  } else {
    m_heap_next = reinterpret_cast<void *>(heap + size);
  }

  if (kDebugHeap > 1) {
    heap = reinterpret_cast<std::uintptr_t>(m_heap_next);
    align = std::min<std::size_t>(align, rx::mem::pageSize);
    heap = (heap + (align - 1)) & ~(align - 1);
    size = rx::mem::pageSize;
    // std::fprintf(stderr, "kalloc: protect %lx-%lx, size = %lx, align=%lx\n",
    //              heap, heap + size, size, align);

    auto errc = rx::mem::protect(rx::AddressRange::fromBeginSize(heap, size),
                                 rx::mem::Protection{});

    rx::dieIf(errc != std::errc{}, "kalloc: failed to protect memory");
    m_heap_next = reinterpret_cast<void *>(heap + size);
  }

  return result;
}

void KernelMemoryResource::kfree(void *ptr, std::size_t size) {
  size = (size + (__STDCPP_DEFAULT_NEW_ALIGNMENT__ - 1)) &
         ~(__STDCPP_DEFAULT_NEW_ALIGNMENT__ - 1);

  rx::dieIf(size == 0, "kfree: zero size");

  rx::dieIf(std::bit_cast<std::uintptr_t>(ptr) < kHeapBaseAddress ||
                std::bit_cast<std::uintptr_t>(ptr) + size >
                    kHeapBaseAddress + kHeapSize,
            "kfree: invalid address");

  if (kDebugHeap > 0) {
    if (std::memcmp(std::bit_cast<std::byte *>(ptr) + size, &g_allocProtWord,
                    sizeof(g_allocProtWord)) != 0) {

      rx::die("kfree: kernel heap corruption");
    }

    std::memset(ptr, 0xcc, size + sizeof(g_allocProtWord));
  }

  // std::fprintf(stderr, "kfree: release %p-%p, size = %lx\n", ptr,
  //              (char *)ptr + size, size);

  std::lock_guard lock(m_heap_map_mtx);
  if (!m_used_node.empty()) {
    auto node = m_used_node.extract(m_used_node.begin());
    node.key() = size;
    node.mapped() = ptr;
    m_free_heap.insert(std::move(node));
  } else {
    m_free_heap.emplace(size, ptr);
  }
}

void kfree(void *ptr, std::size_t size) {
  return sMemoryResource->kfree(ptr, size);
}

void *kalloc(std::size_t size, std::size_t align) {
  return sMemoryResource->kalloc(size, align);
}
} // namespace orbis
