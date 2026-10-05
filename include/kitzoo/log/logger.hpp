// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: include/kitzoo/log/logger.hpp
// Description: Declares the synchronous logger, spdlog sink aliases, and the
//              public logging and check macros.
// -----------------------------------------------------------------------------

#ifndef KITZOO_LOG_LOGGER_HPP
#define KITZOO_LOG_LOGGER_HPP

#include <kitzoo/core/macro.hpp>
#include <kitzoo/memory/memory.hpp>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <iterator>
#include <memory>
#include <mutex>
#include <optional>
#include <source_location>
#include <spdlog/fmt/fmt.h>
#include <spdlog/logger.h>
#include <spdlog/sinks/base_sink.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/callback_sink.h>
#include <spdlog/sinks/daily_file_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <stop_token>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace kitzoo::log {

enum class Level : int {
  Trace = 0,
  Debug = 1,
  Info = 2,
  Warn = 3,
  Error = 4,
  Fatal = 5,
  Off = 6,
};

KZ_NODISCARD constexpr auto to_string(Level level) noexcept -> std::string_view {
  switch (level) {
  case Level::Trace:
    return "TRACE";
  case Level::Debug:
    return "DEBUG";
  case Level::Info:
    return "INFO";
  case Level::Warn:
    return "WARN";
  case Level::Error:
    return "ERROR";
  case Level::Fatal:
    return "FATAL";
  case Level::Off:
    return "OFF";
  }
  return "UNKNOWN";
}

using Sink = spdlog::sinks::sink;
using SinkPtr = spdlog::sink_ptr;
using ConsoleSink = spdlog::sinks::stdout_color_sink_mt;
using FileSink = spdlog::sinks::basic_file_sink_mt;

using StderrSink = spdlog::sinks::stderr_color_sink_mt;
using RotatingFileSink = spdlog::sinks::rotating_file_sink_mt;
using DailyFileSink = spdlog::sinks::daily_file_sink_mt;
using CallbackSink = spdlog::sinks::callback_sink_mt;
using ErrorHandler = std::function<void(std::string_view)>;

struct FileSinkOptions {
  std::filesystem::path path;
  std::size_t max_size_bytes{10 * 1024 * 1024};
  // Zero disables the corresponding retention limit. Counts exclude the active file.
  std::size_t max_files{10};
  std::chrono::seconds max_age{std::chrono::hours{24 * 7}};
  // Zero disables background cleanup; cleanup() remains available.
  std::chrono::milliseconds cleanup_interval{std::chrono::hours{1}};
};

// One sink owns a base path; share that sink when several loggers write to it.
// Generated files use <filename>.kzlog.<session>.<sink>.<sequence> names.
class ManagedFileSink final : public spdlog::sinks::base_sink<std::mutex> {
public:
  explicit ManagedFileSink(const FileSinkOptions& options);

  ~ManagedFileSink() override;

  auto current_file() -> std::filesystem::path;

  // Removes only closed regular files in this sink's naming namespace.
  auto cleanup() -> std::size_t;

  // Latest background cleanup failure; cleared after a successful cleanup.
  auto cleanup_error() -> std::string;

private:
  auto sink_it_(const spdlog::details::log_msg& message) -> void override;

  auto flush_() -> void override;

  auto open_file() -> void;

  auto cleanup_files() -> std::size_t;

  auto cleanup_loop(std::stop_token stop) -> void;

  struct Impl;
  memory::UniquePtr<Impl> impl_;
};

struct LoggerOptions {
  Level level{Level::Info};
  std::string pattern;
  std::vector<SinkPtr> sinks;
  Level flush_level{Level::Off};
  ErrorHandler error_handler;
  std::optional<FileSinkOptions> file;
};

struct LogRecord;

class Logger {
public:
  explicit Logger(std::string name, Level level = Level::Info);

  Logger(std::string name, const LoggerOptions& options);

  Logger(const Logger&) = delete;
  auto operator=(const Logger&) -> Logger& = delete;

  auto add_sink(SinkPtr sink) -> void;

  auto add_file_sink(const FileSinkOptions& options) -> memory::SharedPtr<ManagedFileSink>;

  auto set_pattern(std::string pattern) -> void;

  auto set_level(Level level) noexcept -> void;

  KZ_NODISCARD auto enabled(Level level) const noexcept -> bool { return native_->should_log(to_spdlog(level)); }

  auto log(Level level, std::string_view message, const std::source_location& loc = std::source_location::current())
      -> void;

  template <typename... Args>
  auto logf(Level level, const std::source_location& loc, fmt::format_string<Args...> format, Args&&... args) -> void {
    if (!enabled(level))
      return;
    memory::String message;
    fmt::format_to(std::back_inserter(message), format, std::forward<Args>(args)...);
    log(level, {message.data(), message.size()}, loc);
  }

  auto log_at(std::chrono::system_clock::time_point timestamp, Level level, std::string_view message,
              const std::source_location& loc = std::source_location::current()) -> void;

  auto set_flush_level(Level level) -> void;

  auto set_error_handler(ErrorHandler handler) -> void;

  KZ_NODISCARD auto failed_count() const noexcept -> std::size_t { return failed_.load(); }

  auto flush() -> void;

private:
  friend class AsyncLogger;

  auto write_record(const LogRecord& record) -> bool;

  auto report_error(std::string_view message) noexcept -> void;

  KZ_NODISCARD auto name() const noexcept -> std::string_view { return name_; }

  memory::String name_;
  memory::SharedPtr<spdlog::logger> native_;
  std::string pattern_;
  std::atomic<std::size_t> failed_{0};
  std::mutex error_mutex_;
  ErrorHandler error_handler_;

  static auto to_spdlog(Level level) noexcept -> spdlog::level::level_enum;
};

KZ_NODISCARD auto default_logger() -> Logger&;

} // namespace kitzoo::log

#define KZ_LOG(level, ...)                                                                                             \
  do {                                                                                                                 \
    auto& kitzoo_log_instance = ::kitzoo::log::default_logger();                                                       \
    if (kitzoo_log_instance.enabled(::kitzoo::log::Level::level))                                                      \
      kitzoo_log_instance.logf(::kitzoo::log::Level::level, std::source_location::current(), __VA_ARGS__);             \
  } while (false)
#define KZ_LOG_TRACE(...) KZ_LOG(Trace, __VA_ARGS__)
#define KZ_LOG_DEBUG(...) KZ_LOG(Debug, __VA_ARGS__)
#define KZ_LOG_INFO(...) KZ_LOG(Info, __VA_ARGS__)
#define KZ_LOG_WARN(...) KZ_LOG(Warn, __VA_ARGS__)
#define KZ_LOG_ERROR(...) KZ_LOG(Error, __VA_ARGS__)
#define KZ_LOG_FATAL(...) KZ_LOG(Fatal, __VA_ARGS__)

#define KZ_CHECK_MSG(expr, ...)                                                                                        \
  do {                                                                                                                 \
    if (!static_cast<bool>(expr)) {                                                                                    \
      KZ_LOG_ERROR(__VA_ARGS__);                                                                                       \
      ::kitzoo::log::default_logger().flush();                                                                         \
      std::abort();                                                                                                    \
    }                                                                                                                  \
  } while (false)
#define KZ_CHECK(expr) KZ_CHECK_MSG(expr, "Check failed: {}", #expr)

#endif // KITZOO_LOG_LOGGER_HPP
