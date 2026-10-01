// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: include/kitzoo/lock/spinlock.hpp
// Description: Declares SpinLock and RWSpinLock for lightweight exclusive and
//              reader-writer synchronization.
// -----------------------------------------------------------------------------

#pragma once

#include <kitzoo/core/macro.hpp>

#include <atomic>
#include <cstdint>
#include <thread>

namespace kitzoo::lock {

class SpinLock {
public:
    auto lock() noexcept -> void {
        while (flag_.test_and_set(std::memory_order_acquire))
            std::this_thread::yield();
    }
    KZ_NODISCARD auto try_lock() noexcept -> bool {
        return !flag_.test_and_set(std::memory_order_acquire);
    }
    auto unlock() noexcept -> void { flag_.clear(std::memory_order_release); }

private:
    std::atomic_flag flag_ = ATOMIC_FLAG_INIT;
};

class RWSpinLock {
public:
    auto lock() noexcept -> void {
        while (writer_gate_.test_and_set(std::memory_order_acquire))
            std::this_thread::yield();
        state_.fetch_or(kWriterPending, std::memory_order_acq_rel);
        while ((state_.load(std::memory_order_acquire) & kReaderMask) != 0)
            std::this_thread::yield();
        state_.store(kWriterActive, std::memory_order_release);
    }
    KZ_NODISCARD auto try_lock() noexcept -> bool {
        if (writer_gate_.test_and_set(std::memory_order_acquire))
            return false;
        auto expected = std::uint32_t{0};
        if (state_.compare_exchange_strong(expected, kWriterActive, std::memory_order_acquire,
                                           std::memory_order_relaxed))
            return true;
        writer_gate_.clear(std::memory_order_release);
        return false;
    }
    auto unlock() noexcept -> void {
        state_.store(0, std::memory_order_release);
        writer_gate_.clear(std::memory_order_release);
    }
    auto lock_shared() noexcept -> void {
        auto state = state_.load(std::memory_order_relaxed);
        for (;;) {
            if ((state & kWriterMask) == 0 && (state & kReaderMask) != kReaderMask &&
                state_.compare_exchange_weak(state, state + 1, std::memory_order_acquire,
                                             std::memory_order_relaxed))
                return;
            if ((state & kWriterMask) != 0 || (state & kReaderMask) == kReaderMask) {
                std::this_thread::yield();
                state = state_.load(std::memory_order_relaxed);
            }
        }
    }
    KZ_NODISCARD auto try_lock_shared() noexcept -> bool {
        auto state = state_.load(std::memory_order_relaxed);
        while ((state & kWriterMask) == 0 && (state & kReaderMask) != kReaderMask) {
            if (state_.compare_exchange_weak(state, state + 1, std::memory_order_acquire,
                                             std::memory_order_relaxed))
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

}  // namespace kitzoo::lock
