// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: include/kitzoo/log/async_logger.hpp
// Description: Declares the asynchronous logger, its queued message handling,
//              and shutdown-facing API.
// -----------------------------------------------------------------------------

#ifndef KITZOO_LOG_ASYNC_LOGGER_HPP
#define KITZOO_LOG_ASYNC_LOGGER_HPP

#include <kitzoo/core/macro.hpp>
#include <kitzoo/log/logger.hpp>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <future>
#include <memory>
#include <mutex>
#include <source_location>
#include <string>
#include <string_view>
#include <thread>

namespace kitzoo::log {

struct LogRecord {
  Level level;
  std::chrono::system_clock::time_point timestamp;
  std::thread::id thread_id;
  std::source_location location;
  std::string_view logger_name;
  memory::String message;
};

enum class OverflowPolicy {
  Block,
  DropNewest,
};

struct AsyncLoggerOptions {
  // Records waiting to be taken by the worker; it may hold one more batch while writing.
  std::size_t capacity{8192};
  OverflowPolicy overflow_policy{OverflowPolicy::Block};
};

class AsyncLogger {
public:
  explicit AsyncLogger(std::shared_ptr<Logger> logger, AsyncLoggerOptions options = {});
  ~AsyncLogger();

  AsyncLogger(const AsyncLogger&) = delete;
  auto operator=(const AsyncLogger&) -> AsyncLogger& = delete;

  auto log(Level level, std::string_view message, const std::source_location& loc = std::source_location::current())
      -> void;

  template <typename... Args>
  auto logf(Level level, const std::source_location& loc, fmt::format_string<Args...> format, Args&&... args) -> void {
    if (!logger_->enabled(level))
      return;
    memory::String message;
    fmt::format_to(std::back_inserter(message), format, std::forward<Args>(args)...);
    log_owned(level, std::move(message), loc);
  }

  // Waits for records accepted before this barrier and flushes all sinks.
  auto flush() -> void;

  auto close() -> void;

  // Records submitted after close(). Sink failures are counted by Logger::failed_count().
  KZ_NODISCARD auto rejected_count() const noexcept -> std::size_t { return rejected_.load(); }

  // Records discarded because the queue was full.
  KZ_NODISCARD auto dropped_count() const noexcept -> std::size_t { return dropped_.load(); }

private:
  auto worker_loop() -> void;

  auto log_owned(Level level, memory::String message, const std::source_location& loc) -> void;

  memory::SharedPtr<Logger> logger_;

  struct Work {
    LogRecord record{};
    memory::SharedPtr<std::promise<void>> barrier;
  };

  AsyncLoggerOptions options_;
  std::mutex mutex_;
  std::mutex close_mutex_;
  std::condition_variable available_;
  std::condition_variable space_;
  // Producers append to queue_; the worker swaps it with batch_ and drains the
  // whole batch without the lock, reusing both buffers' storage.
  memory::Vector<Work> queue_;
  memory::Vector<Work> batch_;
  bool closed_{false};
  std::atomic<std::size_t> rejected_{0};
  std::atomic<std::size_t> dropped_{0};
  // Construct the worker after every object it accesses.
  std::jthread worker_;
};

} // namespace kitzoo::log

#endif // KITZOO_LOG_ASYNC_LOGGER_HPP
