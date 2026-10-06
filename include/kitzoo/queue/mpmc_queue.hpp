// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: include/kitzoo/queue/mpmc_queue.hpp
// Description: Declares a bounded lock-free multi-producer, multi-consumer FIFO
//              ring queue with per-slot sequence numbers.
// -----------------------------------------------------------------------------

#ifndef KITZOO_QUEUE_MPMC_QUEUE_HPP
#define KITZOO_QUEUE_MPMC_QUEUE_HPP

#include <kitzoo/core/macro.hpp>
#include <kitzoo/memory/memory.hpp>

#include <algorithm>
#include <atomic>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace kitzoo::queue {

template <typename T>
  requires std::is_nothrow_move_constructible_v<T>
class MPMCQueue {
public:
  // Rounds capacity up to a power of two, at least two.
  explicit MPMCQueue(std::size_t capacity) : mask_{ring_size(capacity) - 1}, slots_(mask_ + 1) {
    for (std::size_t i = 0; i <= mask_; ++i)
      slots_[i].sequence.store(i, std::memory_order_relaxed);
  }

  ~MPMCQueue() {
    while (pop()) {
    }
  }

  MPMCQueue(const MPMCQueue&) = delete;
  auto operator=(const MPMCQueue&) -> MPMCQueue& = delete;
  MPMCQueue(MPMCQueue&&) = delete;
  auto operator=(MPMCQueue&&) -> MPMCQueue& = delete;

  // Returns false without consuming the value when full.
  template <typename U>
    requires std::is_nothrow_constructible_v<T, U&&>
  auto push(U&& value) noexcept -> bool {
    auto pos = head_.load(std::memory_order_relaxed);
    for (;;) {
      auto& slot = slots_[pos & mask_];
      const auto sequence = slot.sequence.load(std::memory_order_acquire);
      const auto diff = static_cast<std::intptr_t>(sequence) - static_cast<std::intptr_t>(pos);
      if (diff == 0) {
        if (head_.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
          std::construct_at(slot.ptr(), std::forward<U>(value));
          slot.sequence.store(pos + 1, std::memory_order_release);
          return true;
        }
      } else if (diff < 0) {
        return false;
      } else {
        pos = head_.load(std::memory_order_relaxed);
      }
    }
  }

  auto pop() noexcept -> std::optional<T> {
    auto pos = tail_.load(std::memory_order_relaxed);
    for (;;) {
      auto& slot = slots_[pos & mask_];
      const auto sequence = slot.sequence.load(std::memory_order_acquire);
      const auto diff = static_cast<std::intptr_t>(sequence) - static_cast<std::intptr_t>(pos + 1);
      if (diff == 0) {
        if (tail_.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
          std::optional<T> value{std::move(*slot.ptr())};
          std::destroy_at(slot.ptr());
          slot.sequence.store(pos + mask_ + 1, std::memory_order_release);
          return value;
        }
      } else if (diff < 0) {
        return std::nullopt;
      } else {
        pos = tail_.load(std::memory_order_relaxed);
      }
    }
  }

  KZ_NODISCARD auto capacity() const noexcept -> std::size_t { return mask_ + 1; }

  // A snapshot; concurrent operations may change it immediately.
  KZ_NODISCARD auto size() const noexcept -> std::size_t {
    const auto tail = tail_.load(std::memory_order_acquire);
    const auto head = head_.load(std::memory_order_acquire);
    return head > tail ? head - tail : 0;
  }

  KZ_NODISCARD auto empty() const noexcept -> bool { return size() == 0; }

private:
  static constexpr std::size_t kCacheLineSize = 128;

  // One slot would let a filled sequence (pos + 1) match the next push position.
  static auto ring_size(std::size_t capacity) -> std::size_t {
    if (capacity == 0 || capacity > (std::size_t{1} << (sizeof(std::size_t) * 8 - 2)))
      throw std::invalid_argument("MPMCQueue capacity must be in [1, SIZE_MAX / 4]");
    return std::bit_ceil(std::max<std::size_t>(capacity, 2));
  }

  struct Slot {
    std::atomic<std::size_t> sequence{0};
    alignas(T) std::byte storage[sizeof(T)];

    auto ptr() noexcept -> T* { return std::launder(reinterpret_cast<T*>(storage)); }
  };

  alignas(kCacheLineSize) std::atomic<std::size_t> head_{0};
  alignas(kCacheLineSize) std::atomic<std::size_t> tail_{0};
  alignas(kCacheLineSize) std::size_t mask_{0};
  memory::Vector<Slot> slots_;
};

} // namespace kitzoo::queue

#endif // KITZOO_QUEUE_MPMC_QUEUE_HPP
