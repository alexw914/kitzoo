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
#include <utility>

namespace kitzoo::log {

namespace {
constexpr auto kDefaultPattern = "[%Y-%m-%d %H:%M:%S.%e] [%l] [tid=%t] [%n] %g:%# %v";
}

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
    : name_{std::move(name)},
      native_{std::make_shared<spdlog::logger>(name_)},
      pattern_{kDefaultPattern} {
    native_->set_level(to_spdlog(level));
}

auto Logger::add_sink(SinkPtr sink) -> void {
    sink->set_pattern(pattern_);
    native_->sinks().push_back(std::move(sink));
}

auto Logger::set_pattern(std::string pattern) -> void {
    pattern_ = std::move(pattern);
    native_->set_pattern(pattern_);
}

auto Logger::set_level(Level const level) noexcept -> void {
    native_->set_level(to_spdlog(level));
}

auto Logger::write_record(LogRecord const& record) -> void {
    auto const loc =
        spdlog::source_loc{record.location.file_name(), static_cast<int>(record.location.line()),
                           record.location.function_name()};
    auto const message = spdlog::string_view_t{record.message.data(), record.message.size()};
    spdlog::details::log_msg msg{record.timestamp, loc, record.logger_name, to_spdlog(record.level),
                                 message};
    msg.thread_id = std::hash<std::thread::id>{}(record.thread_id);
    for (auto const& sink : native_->sinks())
        sink->log(msg);
}

auto Logger::log(Level const level, std::string_view const message, std::source_location const& loc)
    -> void {
    native_->log({loc.file_name(), static_cast<int>(loc.line()), loc.function_name()},
                 to_spdlog(level), {message.data(), message.size()});
}

auto Logger::flush() -> void {
    native_->flush();
}

auto default_logger() -> Logger& {
    static auto* instance = [] {
        auto logger = std::make_unique<Logger>("default");
        logger->add_sink(std::make_shared<ConsoleSink>());
        return logger.release();
    }();
    return *instance;
}

AsyncLogger::AsyncLogger(std::shared_ptr<Logger> logger)
    : logger_{std::move(logger)}, worker_{[this] { worker_loop(); }} {}

AsyncLogger::~AsyncLogger() {
    close();
}

auto AsyncLogger::log(Level const level, std::string_view const message,
                      std::source_location const& loc) -> void {
    if (!logger_->enabled(level))
        return;

    LogRecord record{
        .level = level,
        .timestamp = std::chrono::system_clock::now(),
        .thread_id = std::this_thread::get_id(),
        .location = loc,
        .logger_name = logger_->name(),
        .message = std::string{message},
    };
    if (!queue_.push(std::move(record))) {
        dropped_.fetch_add(1, std::memory_order_relaxed);
    }
}

auto AsyncLogger::worker_loop() -> void {
    while (true) {
        auto record = queue_.wait_and_pop();
        if (!record.has_value())
            return;
        try {
            logger_->write_record(*record);
        } catch (std::exception const& e) {
            std::fprintf(stderr, "AsyncLogger: sink threw: %s\n", e.what());
        } catch (...) {
            std::fprintf(stderr, "AsyncLogger: unknown sink exception\n");
        }
    }
}

auto AsyncLogger::close() -> void {
    queue_.close();
    if (worker_.joinable())
        worker_.join();
    logger_->flush();
}

}  // namespace kitzoo::log
