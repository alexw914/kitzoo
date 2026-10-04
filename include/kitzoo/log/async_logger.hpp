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
#include <kitzoo/queue/blocking_queue.hpp>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <memory>
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
    std::string message;
};

class AsyncLogger {
public:
    explicit AsyncLogger(std::shared_ptr<Logger> logger);
    ~AsyncLogger();

    AsyncLogger(AsyncLogger const&) = delete;
    auto operator=(AsyncLogger const&) -> AsyncLogger& = delete;

    auto log(Level level, std::string_view message,
             std::source_location const& loc = std::source_location::current()) -> void;

    auto close() -> void;

    KZ_NODISCARD auto dropped_count() const noexcept -> std::size_t {
        return dropped_.load(std::memory_order_relaxed);
    }

private:
    auto worker_loop() -> void;

    std::shared_ptr<Logger> logger_;
    queue::BlockingQueue<LogRecord> queue_;
    std::jthread worker_;
    std::atomic<std::size_t> dropped_{0};
};

}  // namespace kitzoo::log

#endif  // KITZOO_LOG_ASYNC_LOGGER_HPP
