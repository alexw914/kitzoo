// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: include/kitzoo/time/timer.hpp
// Description: Declares an interruptible periodic timer that invokes a callback
//              on its worker thread until stopped.
// -----------------------------------------------------------------------------

#ifndef KITZOO_TIME_TIMER_HPP
#define KITZOO_TIME_TIMER_HPP

#include <kitzoo/core/macro.hpp>
#include <kitzoo/core/unique_function.hpp>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace kitzoo::time {

namespace detail {
// First tick on the scheduled grid strictly after now; ticks missed by a slow
// callback are skipped rather than fired back to back.
KZ_NODISCARD auto next_tick(std::chrono::steady_clock::time_point scheduled, std::chrono::steady_clock::time_point now,
                            std::chrono::milliseconds interval) noexcept -> std::chrono::steady_clock::time_point;
} // namespace detail

class Timer {
public:
  explicit Timer(std::chrono::milliseconds interval);

  ~Timer();

  Timer(const Timer&) = delete;
  auto operator=(const Timer&) -> Timer& = delete;

  // Control start/stop from the owning thread; stop joins the worker.
  // A callback may call stop(); the owner joins the worker later.
  // Callbacks run at a fixed rate; ticks missed by a slow callback are skipped.
  // Callbacks must not throw: an escaping exception terminates the process.
  auto start(kitzoo::core::unique_function<void()> callback) -> void;

  auto stop() noexcept -> void;

  KZ_NODISCARD auto running() const noexcept -> bool;

private:
  std::chrono::milliseconds interval_;
  kitzoo::core::unique_function<void()> callback_;
  std::jthread worker_;
  std::mutex mutex_;
  std::condition_variable_any cv_;
  std::atomic<bool> running_{false};
};

} // namespace kitzoo::time

#endif // KITZOO_TIME_TIMER_HPP
