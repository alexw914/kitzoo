// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: src/log/log.cpp
// Description: Implements kitzoo logger construction, configuration, message
//              dispatch, and shutdown behavior.
// -----------------------------------------------------------------------------

#include <kitzoo/log/async_logger.hpp>
#include <kitzoo/log/logger.hpp>

#include <cstdio>
#include <functional>
#include <memory>
#include <spdlog/details/log_msg.h>
#include <stdexcept>
#include <utility>

namespace kitzoo::log {

namespace {
thread_local AsyncLogger* active_worker = nullptr;
thread_local Logger* active_error_handler = nullptr;
constexpr auto kDefaultPattern = "[%Y-%m-%d %H:%M:%S.%e] [%l] [tid=%t] [%n] %g:%# %v";
} // namespace

auto Logger::to_spdlog(Level level) noexcept -> spdlog::level::level_enum {
  static_assert(static_cast<int>(Level::Trace) == static_cast<int>(spdlog::level::trace));
  static_assert(static_cast<int>(Level::Debug) == static_cast<int>(spdlog::level::debug));
  static_assert(static_cast<int>(Level::Info) == static_cast<int>(spdlog::level::info));
  static_assert(static_cast<int>(Level::Warn) == static_cast<int>(spdlog::level::warn));
  static_assert(static_cast<int>(Level::Error) == static_cast<int>(spdlog::level::err));
  static_assert(static_cast<int>(Level::Fatal) == static_cast<int>(spdlog::level::critical));
  return static_cast<spdlog::level::level_enum>(static_cast<int>(level));
}

Logger::Logger(std::string name, Level const level)
    : name_{name.data(), name.size()}, native_{memory::make_shared<spdlog::logger>(std::move(name))},
      pattern_{kDefaultPattern} {
  native_->set_level(to_spdlog(level));
  native_->set_pattern(std::string(pattern_.data(), pattern_.size()));
  native_->set_error_handler([this](const std::string& message) { report_error(message); });
}

Logger::Logger(std::string name, const LoggerOptions& options) : Logger(std::move(name), options.level) {
  if (!options.pattern.empty())
    set_pattern(options.pattern);
  for (const auto& sink : options.sinks)
    add_sink(sink);
  if (options.file)
    add_file_sink(*options.file);
  set_flush_level(options.flush_level);
  set_error_handler(options.error_handler);
}

auto Logger::set_flush_level(Level level) -> void {
  native_->flush_on(to_spdlog(level));
}

auto Logger::set_error_handler(ErrorHandler handler) -> void {
  std::lock_guard lock(error_mutex_);
  error_handler_ = std::move(handler);
}

auto Logger::report_error(std::string_view message) noexcept -> void {
  failed_.fetch_add(1, std::memory_order_relaxed);
  if (active_error_handler == this)
    return;
  auto* previous = active_error_handler;
  active_error_handler = this;
  try {
    ErrorHandler handler;
    {
      std::lock_guard lock(error_mutex_);
      handler = error_handler_;
    }
    if (handler)
      handler(message);
    else
      std::fprintf(stderr, "Logger: %.*s\n", static_cast<int>(message.size()), message.data());
  } catch (...) {
  }
  active_error_handler = previous;
}

auto Logger::add_sink(SinkPtr sink) -> void {
  if (!sink)
    throw std::invalid_argument("sink must not be null");
  sink->set_pattern(std::string(pattern_.data(), pattern_.size()));
  native_->sinks().push_back(std::move(sink));
}

auto Logger::add_file_sink(FileSinkOptions const& options) -> memory::SharedPtr<ManagedFileSink> {
  auto sink = memory::make_shared<ManagedFileSink>(options);
  add_sink(sink);
  return sink;
}

auto Logger::set_pattern(std::string pattern) -> void {
  pattern_.assign(pattern.data(), pattern.size());
  native_->set_pattern(std::string(pattern_.data(), pattern_.size()));
}

auto Logger::set_level(Level const level) noexcept -> void {
  native_->set_level(to_spdlog(level));
}

auto Logger::write_record(LogRecord const& record) -> bool {
  auto const loc = spdlog::source_loc{record.location.file_name(), static_cast<int>(record.location.line()),
                                      record.location.function_name()};
  auto const message = spdlog::string_view_t{record.message.data(), record.message.size()};
  spdlog::details::log_msg msg{record.timestamp, loc, record.logger_name, to_spdlog(record.level), message};
  msg.thread_id = std::hash<std::thread::id>{}(record.thread_id);
  bool success = true;
  for (auto const& sink : native_->sinks()) {
    if (!sink->should_log(msg.level))
      continue;
    try {
      sink->log(msg);
    } catch (const std::exception& error) {
      success = false;
      report_error(error.what());
    } catch (...) {
      success = false;
      report_error("unknown sink exception");
    }
  }
  if (msg.level >= native_->flush_level())
    flush();
  return success;
}

auto Logger::log(Level const level, std::string_view const message, std::source_location const& loc) -> void {
  native_->log({loc.file_name(), static_cast<int>(loc.line()), loc.function_name()}, to_spdlog(level),
               {message.data(), message.size()});
}

auto Logger::log_at(std::chrono::system_clock::time_point timestamp, Level level, std::string_view message,
                    const std::source_location& loc) -> void {
  native_->log(timestamp, {loc.file_name(), static_cast<int>(loc.line()), loc.function_name()}, to_spdlog(level),
               {message.data(), message.size()});
}

auto Logger::flush() -> void {
  native_->flush();
}

auto default_logger() -> Logger& {
  static auto* instance = [] {
    auto logger = memory::make_unique<Logger>("default");
    logger->add_sink(memory::make_shared<ConsoleSink>());
    return logger.release();
  }();
  return *instance;
}

AsyncLogger::AsyncLogger(std::shared_ptr<Logger> logger, AsyncLoggerOptions options)
    : logger_{std::move(logger)}, options_{options} {
  if (!logger_)
    throw std::invalid_argument("AsyncLogger requires a logger");
  if (!options_.capacity)
    throw std::invalid_argument("queue capacity must be positive");
  worker_ = std::jthread([this] { worker_loop(); });
}

AsyncLogger::~AsyncLogger() {
  close();
}

auto AsyncLogger::log(Level level, std::string_view message, const std::source_location& loc) -> void {
  if (!logger_->enabled(level))
    return;
  log_owned(level, memory::String(message.data(), message.size()), loc);
}

auto AsyncLogger::log_owned(Level level, memory::String message, const std::source_location& loc) -> void {
  LogRecord record{
      level, std::chrono::system_clock::now(), std::this_thread::get_id(), loc, logger_->name(), std::move(message)};
  std::unique_lock lock(mutex_);
  if (closed_) {
    ++rejected_;
    ++dropped_;
    return;
  }
  if (queue_.size() >= options_.capacity) {
    // A callback on this worker must never wait for its own queue.
    if (options_.overflow_policy == OverflowPolicy::DropNewest || active_worker == this) {
      ++dropped_;
      return;
    }
    space_.wait(lock, [this] { return closed_ || queue_.size() < options_.capacity; });
    if (closed_) {
      ++rejected_;
      ++dropped_;
      return;
    }
  }
  queue_.push_back({std::move(record), {}});
  lock.unlock();
  available_.notify_one();
}

auto AsyncLogger::flush() -> void {
  if (active_worker == this)
    throw std::logic_error("cannot flush from the logger worker");
  auto barrier = memory::make_shared<std::promise<void>>();
  auto complete = barrier->get_future();
  std::unique_lock lock(mutex_);
  space_.wait(lock, [this] { return closed_ || queue_.size() < options_.capacity; });
  if (closed_) {
    lock.unlock();
    close();
    return;
  }
  queue_.push_back({{}, std::move(barrier)});
  lock.unlock();
  available_.notify_one();
  complete.get();
}

auto AsyncLogger::worker_loop() -> void {
  active_worker = this;
  for (;;) {
    Work work;
    {
      std::unique_lock lock(mutex_);
      available_.wait(lock, [this] { return closed_ || !queue_.empty(); });
      if (queue_.empty())
        break;
      work = std::move(queue_.front());
      queue_.pop_front();
    }
    space_.notify_all();
    if (work.barrier) {
      try {
        logger_->flush();
        work.barrier->set_value();
      } catch (...) {
        ++failed_;
        work.barrier->set_exception(std::current_exception());
      }
    } else {
      try {
        if (!logger_->write_record(work.record))
          ++failed_;
      } catch (const std::exception& error) {
        ++failed_;
        logger_->report_error(error.what());
      } catch (...) {
        ++failed_;
        logger_->report_error("unknown worker exception");
      }
    }
  }
  active_worker = nullptr;
}

auto AsyncLogger::close() -> void {
  if (active_worker == this)
    throw std::logic_error("cannot close from the logger worker");
  std::lock_guard closing(close_mutex_);
  {
    std::lock_guard lock(mutex_);
    closed_ = true;
  }
  available_.notify_all();
  space_.notify_all();
  if (worker_.joinable()) {
    worker_.join();
    logger_->flush();
  }
}

} // namespace kitzoo::log
