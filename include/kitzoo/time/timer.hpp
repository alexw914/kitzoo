// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: include/kitzoo/time/timer.hpp
// Description: Declares an interruptible periodic timer that invokes a callback
//              on its worker thread until stopped.
// -----------------------------------------------------------------------------

#ifndef KITZOO_TIME_TIMER_HPP
#define KITZOO_TIME_TIMER_HPP

#include <kitzoo/core/macro.hpp>
#include <kitzoo/utilities/unique_function.hpp>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <utility>

namespace kitzoo::time {

class Timer {
public:
    explicit Timer(std::chrono::milliseconds interval) : interval_{interval} {
        if (interval_ <= std::chrono::milliseconds::zero())
            throw std::invalid_argument{"timer interval must be positive"};
    }

    ~Timer() { stop(); }

    Timer(Timer const&) = delete;
    auto operator=(Timer const&) -> Timer& = delete;

    auto start(kitzoo::util::unique_function<void()> callback) -> void {
        if (running_.exchange(true, std::memory_order_acq_rel))
            return;
        if (worker_.joinable())
            worker_.join();
        callback_ = std::move(callback);
        worker_ = std::jthread{[this](std::stop_token token) {
            std::unique_lock lock{mutex_};
            while (!token.stop_requested()) {
                cv_.wait_for(lock, token, interval_, [] { return false; });
                if (token.stop_requested())
                    break;
                lock.unlock();
                try {
                    callback_();
                } catch (...) {
                }
                lock.lock();
            }
            running_.store(false, std::memory_order_release);
        }};
    }

    auto stop() noexcept -> void {
        if (worker_.joinable()) {
            worker_.request_stop();
            cv_.notify_all();
            worker_.join();
        }
        running_.store(false, std::memory_order_release);
    }

    KZ_NODISCARD auto running() const noexcept -> bool {
        return running_.load(std::memory_order_acquire);
    }

private:
    std::chrono::milliseconds interval_;
    kitzoo::util::unique_function<void()> callback_;
    std::jthread worker_;
    std::mutex mutex_;
    std::condition_variable_any cv_;
    std::atomic<bool> running_{false};
};

}  // namespace kitzoo::time

#endif  // KITZOO_TIME_TIMER_HPP
