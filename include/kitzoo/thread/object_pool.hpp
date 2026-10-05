// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: include/kitzoo/thread/object_pool.hpp
// Description: Declares concurrent and local object pools with leases that
//              return objects to their pool on release.
// -----------------------------------------------------------------------------

#ifndef KITZOO_THREAD_OBJECT_POOL_HPP
#define KITZOO_THREAD_OBJECT_POOL_HPP

#include <kitzoo/core/macro.hpp>
#include <kitzoo/memory/memory.hpp>
#include <kitzoo/queue/concurrent_queue.hpp>

#include <chrono>
#include <cstddef>
#include <memory>
#include <new>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace kitzoo::thread {

// Concurrent pool of existing objects; leases return objects without destroying them.
template <typename T>
class ObjectPool {
public:
  struct Deleter {
    ObjectPool* pool;

    auto operator()(T* object) const noexcept -> void {
      if (object != nullptr)
        (void)pool->objects_.enqueue(memory::UniquePtr<T>{object});
    }
  };

  ObjectPool() = default;
  ObjectPool(const ObjectPool&) = delete;
  auto operator=(const ObjectPool&) -> ObjectPool& = delete;
  ObjectPool(ObjectPool&&) = delete;
  auto operator=(ObjectPool&&) -> ObjectPool& = delete;

  auto add(memory::UniquePtr<T> object) -> void {
    if (!object)
      throw std::invalid_argument{"cannot add a null object"};
    if (!objects_.enqueue(std::move(object)))
      throw std::bad_alloc{};
  }

  KZ_NODISCARD auto acquire() -> memory::UniquePtr<T, Deleter> {
    memory::UniquePtr<T> object;
    objects_.wait_dequeue(object);
    return memory::UniquePtr<T, Deleter>{object.release(), Deleter{this}};
  }

  KZ_NODISCARD auto try_acquire() -> std::optional<memory::UniquePtr<T, Deleter>> {
    memory::UniquePtr<T> object;
    if (!objects_.try_dequeue(object))
      return std::nullopt;
    return memory::UniquePtr<T, Deleter>{object.release(), Deleter{this}};
  }

  template <typename Rep, typename Period>
  KZ_NODISCARD auto acquire_for(std::chrono::duration<Rep, Period> timeout)
      -> std::optional<memory::UniquePtr<T, Deleter>> {
    memory::UniquePtr<T> object;
    if (!objects_.wait_dequeue_timed(object, timeout))
      return std::nullopt;
    return memory::UniquePtr<T, Deleter>{object.release(), Deleter{this}};
  }

  KZ_NODISCARD auto acquire_shared() -> memory::SharedPtr<T> {
    auto object = acquire();
    return memory::SharedPtr<T>{object.release(), Deleter{this}, memory::MiAllocator<T>{}};
  }

  KZ_NODISCARD auto available_approx() const noexcept -> std::size_t { return objects_.size_approx(); }

private:
  kitzoo::queue::BlockingConcurrentQueue<memory::UniquePtr<T>> objects_;
};

// Local storage pool; callers synchronize access and each lease constructs a new object.
template <typename T>
class LocalObjectPool {
  union Slot {
    Slot* next;
    alignas(T) std::byte storage[sizeof(T)];
  };

public:
  struct Deleter {
    LocalObjectPool* pool;

    auto operator()(T* ptr) const noexcept -> void { pool->destroy(ptr); }
  };

  explicit LocalObjectPool(std::size_t chunk_size = 64) : chunk_size_{chunk_size == 0 ? 64 : chunk_size} {}

  LocalObjectPool(const LocalObjectPool&) = delete;
  auto operator=(const LocalObjectPool&) -> LocalObjectPool& = delete;
  auto operator=(LocalObjectPool&&) -> LocalObjectPool& = delete;
  LocalObjectPool(LocalObjectPool&&) = delete;

  template <typename... Args>
  KZ_NODISCARD auto acquire(Args&&... args) -> memory::UniquePtr<T, Deleter> {
    return memory::UniquePtr<T, Deleter>{construct(std::forward<Args>(args)...), Deleter{this}};
  }

  template <typename... Args>
  KZ_NODISCARD auto acquire_shared(Args&&... args) -> memory::SharedPtr<T> {
    auto object = acquire(std::forward<Args>(args)...);
    return memory::SharedPtr<T>{object.release(), Deleter{this}, memory::MiAllocator<T>{}};
  }

  template <typename... Args>
  KZ_NODISCARD auto construct(Args&&... args) -> T* {
    if (free_ == nullptr)
      grow();
    Slot* slot = free_;
    free_ = slot->next;
    auto* object = std::launder(reinterpret_cast<T*>(slot->storage));
    try {
      std::construct_at(object, std::forward<Args>(args)...);
    } catch (...) {
      slot->next = free_;
      free_ = slot;
      throw;
    }
    ++live_;
    return object;
  }

  auto destroy(T* object) noexcept -> void {
    if (object == nullptr)
      return;
    std::destroy_at(object);
    auto* slot = reinterpret_cast<Slot*>(object);
    slot->next = free_;
    free_ = slot;
    --live_;
  }

  KZ_NODISCARD auto allocated_count() const noexcept -> std::size_t { return live_; }

private:
  auto grow() -> void {
    memory::Vector<Slot> chunk(chunk_size_);
    auto* slots = chunk.data();
    // Commit ownership before publishing slots in the free list.
    chunks_.push_back(std::move(chunk));
    for (std::size_t i = 0; i + 1 < chunk_size_; ++i)
      slots[i].next = &slots[i + 1];
    slots[chunk_size_ - 1].next = nullptr;
    free_ = slots;
  }

  std::size_t chunk_size_;
  memory::Vector<memory::Vector<Slot>> chunks_;
  Slot* free_{nullptr};
  std::size_t live_{0};
};

} // namespace kitzoo::thread

#endif // KITZOO_THREAD_OBJECT_POOL_HPP
