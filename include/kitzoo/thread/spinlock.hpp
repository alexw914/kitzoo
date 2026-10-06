// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: include/kitzoo/thread/spinlock.hpp
// Description: Declares SpinLock and RWSpinLock for lightweight exclusive and
//              reader-writer synchronization.
// -----------------------------------------------------------------------------

#ifndef KITZOO_THREAD_SPINLOCK_HPP
#define KITZOO_THREAD_SPINLOCK_HPP

#include <kitzoo/core/macro.hpp>

#include <atomic>
#include <cstdint>
#include <thread>

#if defined(_MSC_VER)
#include <intrin.h>
#endif

namespace kitzoo::thread {

namespace detail {

KZ_ALWAYS_INLINE auto cpu_relax() noexcept -> void {
#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
  _mm_pause();
#elif defined(_MSC_VER) && defined(_M_ARM64)
  __yield();
#elif defined(__x86_64__) || defined(__i386__)
  __builtin_ia32_pause();
#elif defined(__aarch64__) || defined(__arm__)
  __asm__ __volatile__("yield");
#endif
}

// Doubles the pause count per round, then yields once the spin budget is spent.
class Backoff {
public:
  auto pause() noexcept -> void {
    if (spins_ > kMaxSpins) {
      std::this_thread::yield();
      return;
    }
    for (std::uint32_t i = 0; i < spins_; ++i)
      cpu_relax();
    spins_ *= 2;
  }

private:
  static constexpr std::uint32_t kMaxSpins = 64;
  std::uint32_t spins_{1};
};

} // namespace detail

class SpinLock {
public:
  // Waiters spin on a load so the cache line stays shared until the lock is released.
  auto lock() noexcept -> void {
    detail::Backoff backoff;
    while (flag_.test_and_set(std::memory_order_acquire)) {
      while (flag_.test(std::memory_order_relaxed))
        backoff.pause();
    }
  }

  KZ_NODISCARD auto try_lock() noexcept -> bool { return !flag_.test_and_set(std::memory_order_acquire); }

  auto unlock() noexcept -> void { flag_.clear(std::memory_order_release); }

private:
  std::atomic_flag flag_ = ATOMIC_FLAG_INIT;
};

class RWSpinLock {
public:
  auto lock() noexcept -> void {
    detail::Backoff backoff;
    while (writer_gate_.test_and_set(std::memory_order_acquire)) {
      while (writer_gate_.test(std::memory_order_relaxed))
        backoff.pause();
    }
    state_.fetch_or(kWriterPending, std::memory_order_acq_rel);
    detail::Backoff drain;
    while ((state_.load(std::memory_order_acquire) & kReaderMask) != 0)
      drain.pause();
    state_.store(kWriterActive, std::memory_order_release);
  }

  KZ_NODISCARD auto try_lock() noexcept -> bool {
    if (writer_gate_.test_and_set(std::memory_order_acquire))
      return false;
    auto expected = std::uint32_t{0};
    if (state_.compare_exchange_strong(expected, kWriterActive, std::memory_order_acquire, std::memory_order_relaxed))
      return true;
    writer_gate_.clear(std::memory_order_release);
    return false;
  }

  auto unlock() noexcept -> void {
    state_.store(0, std::memory_order_release);
    writer_gate_.clear(std::memory_order_release);
  }

  auto lock_shared() noexcept -> void {
    detail::Backoff backoff;
    auto state = state_.load(std::memory_order_relaxed);
    for (;;) {
      if ((state & kWriterMask) == 0 && (state & kReaderMask) != kReaderMask &&
          state_.compare_exchange_weak(state, state + 1, std::memory_order_acquire, std::memory_order_relaxed))
        return;
      if ((state & kWriterMask) != 0 || (state & kReaderMask) == kReaderMask) {
        backoff.pause();
        state = state_.load(std::memory_order_relaxed);
      }
    }
  }

  KZ_NODISCARD auto try_lock_shared() noexcept -> bool {
    auto state = state_.load(std::memory_order_relaxed);
    while ((state & kWriterMask) == 0 && (state & kReaderMask) != kReaderMask) {
      if (state_.compare_exchange_weak(state, state + 1, std::memory_order_acquire, std::memory_order_relaxed))
        return true;
    }
    return false;
  }

  auto unlock_shared() noexcept -> void { state_.fetch_sub(1, std::memory_order_release); }

private:
  static constexpr std::uint32_t kWriterActive = std::uint32_t{1} << 31;
  static constexpr std::uint32_t kWriterPending = std::uint32_t{1} << 30;
  static constexpr std::uint32_t kWriterMask = kWriterActive | kWriterPending;
  static constexpr std::uint32_t kReaderMask = kWriterPending - 1;
  std::atomic<std::uint32_t> state_{0};
  std::atomic_flag writer_gate_ = ATOMIC_FLAG_INIT;
};

} // namespace kitzoo::thread

#endif // KITZOO_THREAD_SPINLOCK_HPP
