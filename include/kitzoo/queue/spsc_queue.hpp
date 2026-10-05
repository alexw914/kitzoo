// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: include/kitzoo/queue/spsc_queue.hpp
// Description: Declares a bounded single-producer, single-consumer ring queue
//              with acquire-release synchronization.
// -----------------------------------------------------------------------------

#ifndef KITZOO_QUEUE_SPSC_QUEUE_HPP
#define KITZOO_QUEUE_SPSC_QUEUE_HPP

#include <kitzoo/core/macro.hpp>

#include <atomic>
#include <cstddef>
#include <memory>
#include <new>
#include <optional>
#include <type_traits>
#include <utility>

namespace kitzoo::queue {

template <typename T, std::size_t Capacity>
class SPSCQueue {
  static constexpr std::size_t kCacheLineSize = 128;

  static_assert(Capacity >= 2, "SPSCQueue needs Capacity >= 2");
  static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be a power of 2");
  static_assert(std::is_nothrow_move_constructible_v<T>, "T must be nothrow move constructible");

  static constexpr std::size_t kMask = Capacity - 1;

  alignas(kCacheLineSize) std::atomic<std::size_t> write_idx_{0};

  alignas(kCacheLineSize) std::atomic<std::size_t> read_idx_{0};

  alignas(T) std::byte buffer_[Capacity * sizeof(T)];

  auto slot(std::size_t idx) noexcept -> T* {
    return std::launder(reinterpret_cast<T*>(buffer_ + (idx & kMask) * sizeof(T)));
  }

public:
  SPSCQueue() = default;

  ~SPSCQueue() {
    auto r = read_idx_.load(std::memory_order_relaxed);
    const auto w = write_idx_.load(std::memory_order_relaxed);
    while (r != w) {
      std::destroy_at(slot(r));
      ++r;
    }
  }

  SPSCQueue(const SPSCQueue&) = delete;
  auto operator=(const SPSCQueue&) -> SPSCQueue& = delete;
  SPSCQueue(SPSCQueue&&) = delete;
  auto operator=(SPSCQueue&&) -> SPSCQueue& = delete;

  auto push(T value) noexcept -> bool {
    const auto w = write_idx_.load(std::memory_order_relaxed);
    const auto r = read_idx_.load(std::memory_order_acquire);
    if (w - r == Capacity)
      return false;
    std::construct_at(slot(w), std::move(value));
    write_idx_.store(w + 1, std::memory_order_release);
    return true;
  }

  auto pop() noexcept(std::is_nothrow_move_constructible_v<T>) -> std::optional<T> {
    const auto r = read_idx_.load(std::memory_order_relaxed);
    const auto w = write_idx_.load(std::memory_order_acquire);
    if (r == w)
      return std::nullopt;
    std::optional<T> value{std::move(*slot(r))};
    std::destroy_at(slot(r));
    read_idx_.store(r + 1, std::memory_order_release);
    return value;
  }

  KZ_NODISCARD static consteval auto capacity() noexcept -> std::size_t { return Capacity; }

  // A snapshot; the other thread may change it immediately.
  KZ_NODISCARD auto size() const noexcept -> std::size_t {
    return write_idx_.load(std::memory_order_acquire) - read_idx_.load(std::memory_order_acquire);
  }

  KZ_NODISCARD auto empty() const noexcept -> bool {
    return read_idx_.load(std::memory_order_relaxed) == write_idx_.load(std::memory_order_relaxed);
  }
};

} // namespace kitzoo::queue

#endif // KITZOO_QUEUE_SPSC_QUEUE_HPP
