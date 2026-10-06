// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: include/kitzoo/queue/blocking_queue.hpp
// Description: Declares a closable multi-producer, multi-consumer queue whose
//              consumers can wait for items and producers can be bounded.
// -----------------------------------------------------------------------------

#ifndef KITZOO_QUEUE_BLOCKING_QUEUE_HPP
#define KITZOO_QUEUE_BLOCKING_QUEUE_HPP

#include <kitzoo/core/macro.hpp>
#include <kitzoo/memory/memory.hpp>

#include <chrono>
#include <concepts>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <optional>
#include <utility>

namespace kitzoo::queue {

template <typename T>
class BlockingQueue {
public:
  // Zero capacity means unbounded.
  explicit BlockingQueue(std::size_t capacity = 0) : capacity_{capacity} {}

  ~BlockingQueue() = default;

  BlockingQueue(const BlockingQueue&) = delete;
  auto operator=(const BlockingQueue&) -> BlockingQueue& = delete;
  BlockingQueue(BlockingQueue&&) = delete;
  auto operator=(BlockingQueue&&) -> BlockingQueue& = delete;

  // Blocks while a bounded queue is full; returns false once closed.
  auto push(T value) -> bool {
    {
      std::unique_lock lock{mutex_};
      not_full_.wait(lock, [this] { return closed_ || !full(); });
      if (closed_)
        return false;
      deque_.push_back(std::move(value));
    }
    not_empty_.notify_one();
    return true;
  }

  // Returns false without consuming the value when full or closed.
  template <typename U>
    requires std::constructible_from<T, U&&>
  KZ_NODISCARD auto try_push(U&& value) -> bool {
    {
      std::lock_guard lock{mutex_};
      if (closed_ || full())
        return false;
      deque_.emplace_back(std::forward<U>(value));
    }
    not_empty_.notify_one();
    return true;
  }

  // Never blocks. When a bounded queue is full, the oldest item makes room and
  // is returned; once closed, value itself is returned. The returned item is
  // the one that did not end up in the queue.
  auto push_evict(T value) -> std::optional<T> {
    std::optional<T> evicted;
    {
      std::lock_guard lock{mutex_};
      if (closed_)
        return value;
      deque_.push_back(std::move(value));
      if (capacity_ != 0 && deque_.size() > capacity_) {
        evicted.emplace(std::move(deque_.front()));
        deque_.pop_front();
      }
    }
    not_empty_.notify_one();
    return evicted;
  }

  KZ_NODISCARD auto wait_and_pop() -> std::optional<T> {
    std::unique_lock lock{mutex_};
    not_empty_.wait(lock, [this] { return !deque_.empty() || closed_; });
    return take(lock);
  }

  // Returns nullopt on timeout or when the queue is closed and drained.
  template <typename Rep, typename Period>
  KZ_NODISCARD auto pop_for(std::chrono::duration<Rep, Period> timeout) -> std::optional<T> {
    std::unique_lock lock{mutex_};
    not_empty_.wait_for(lock, timeout, [this] { return !deque_.empty() || closed_; });
    return take(lock);
  }

  KZ_NODISCARD auto try_pop() -> std::optional<T> {
    std::unique_lock lock{mutex_};
    return take(lock);
  }

  auto close() -> void {
    {
      std::lock_guard lock{mutex_};
      closed_ = true;
    }
    not_empty_.notify_all();
    not_full_.notify_all();
  }

  KZ_NODISCARD auto is_closed() const -> bool {
    std::lock_guard lock{mutex_};
    return closed_;
  }

  KZ_NODISCARD auto empty() const -> bool {
    std::lock_guard lock{mutex_};
    return deque_.empty();
  }

  KZ_NODISCARD auto size() const -> std::size_t {
    std::lock_guard lock{mutex_};
    return deque_.size();
  }

  KZ_NODISCARD auto capacity() const noexcept -> std::size_t { return capacity_; }

private:
  auto full() const -> bool { return capacity_ != 0 && deque_.size() >= capacity_; }

  auto take(std::unique_lock<std::mutex>& lock) -> std::optional<T> {
    if (deque_.empty())
      return std::nullopt;
    auto value = std::move(deque_.front());
    deque_.pop_front();
    lock.unlock();
    not_full_.notify_one();
    return value;
  }

  const std::size_t capacity_;
  mutable std::mutex mutex_;
  std::condition_variable not_empty_;
  std::condition_variable not_full_;
  memory::Deque<T> deque_;
  bool closed_{false};
};

} // namespace kitzoo::queue

#endif // KITZOO_QUEUE_BLOCKING_QUEUE_HPP
