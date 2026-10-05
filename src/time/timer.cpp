// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: src/time/timer.cpp
// Description: Implements periodic callbacks and interruptible worker shutdown.
// -----------------------------------------------------------------------------

#include <kitzoo/time/timer.hpp>

#include <stdexcept>
#include <utility>

namespace kitzoo::time {

Timer::Timer(std::chrono::milliseconds interval) : interval_{interval} {
  if (interval_ <= std::chrono::milliseconds::zero())
    throw std::invalid_argument{"timer interval must be positive"};
}

Timer::~Timer() {
  stop();
}

auto Timer::start(kitzoo::core::unique_function<void()> callback) -> void {
  if (running_.exchange(true, std::memory_order_acq_rel))
    return;
  if (worker_.joinable())
    worker_.join();
  callback_ = std::move(callback);
  worker_ = std::jthread{[this](std::stop_token token) -> void {
    std::unique_lock lock{mutex_};
    while (!token.stop_requested()) {
      cv_.wait_for(lock, token, interval_, []() -> bool { return false; });
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

auto Timer::stop() noexcept -> void {
  if (worker_.joinable()) {
    worker_.request_stop();
    cv_.notify_all();
    worker_.join();
  }
  running_.store(false, std::memory_order_release);
}

auto Timer::running() const noexcept -> bool {
  return running_.load(std::memory_order_acquire);
}

} // namespace kitzoo::time
