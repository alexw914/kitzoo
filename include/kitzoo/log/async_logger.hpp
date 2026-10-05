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
#include <deque>
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
  detail::Message message;
};

enum class OverflowPolicy {
  Block,
  DropNewest,
};

struct AsyncLoggerOptions {
  std::size_t capacity{8192};
  OverflowPolicy overflow_policy{OverflowPolicy::Block};
};

class AsyncLogger {
public:
  explicit AsyncLogger(std::shared_ptr<Logger> logger, AsyncLoggerOptions options = {});
  ~AsyncLogger();

  AsyncLogger(AsyncLogger const&) = delete;
  auto operator=(AsyncLogger const&) -> AsyncLogger& = delete;

  auto log(Level level, std::string_view message,
           std::source_location const& loc = std::source_location::current()) -> void;

  template <typename... Args>
  auto logf(Level level, const std::source_location& loc, fmt::format_string<Args...> format, Args&&... args) -> void {
    if (!logger_->enabled(level))
      return;
    detail::Message message;
    fmt::format_to(std::back_inserter(message), format, std::forward<Args>(args)...);
    log_owned(level, std::move(message), loc);
  }

  // Waits for records accepted before this barrier and flushes all sinks.
  auto flush() -> void;

  auto close() -> void;

  auto rejected_count() const noexcept -> std::size_t { return rejected_.load(); }

  auto failed_count() const noexcept -> std::size_t { return failed_.load(); }

  KZ_NODISCARD auto dropped_count() const noexcept -> std::size_t { return dropped_.load(std::memory_order_relaxed); }

private:
  auto worker_loop() -> void;

  auto log_owned(Level level, detail::Message message, const std::source_location& loc) -> void;

  std::shared_ptr<Logger> logger_;

  struct Work {
    LogRecord record{};
    std::shared_ptr<std::promise<void>> barrier;
  };

  AsyncLoggerOptions options_;
  std::mutex mutex_;
  std::mutex close_mutex_;
  std::condition_variable available_;
  std::condition_variable space_;
  memory::Deque<Work> queue_;
  bool closed_{false};
  std::atomic<std::size_t> rejected_{0};
  std::atomic<std::size_t> failed_{0};
  std::atomic<std::size_t> dropped_{0};
  // Construct the worker after every object it accesses.
  std::jthread worker_;
};

} // namespace kitzoo::log

#endif // KITZOO_LOG_ASYNC_LOGGER_HPP
