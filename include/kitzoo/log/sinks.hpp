// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: include/kitzoo/log/sinks.hpp
// Description: Declares console, file and callback sinks and the managed
//              rolling file sink used by loggers.
// -----------------------------------------------------------------------------

#ifndef KITZOO_LOG_SINKS_HPP
#define KITZOO_LOG_SINKS_HPP

#include <kitzoo/log/logger.hpp>

#include <cstddef>
#include <filesystem>
#include <mutex>
#include <spdlog/sinks/base_sink.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/callback_sink.h>
#include <spdlog/sinks/daily_file_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <stop_token>
#include <string>

namespace kitzoo::log {

using ConsoleSink = spdlog::sinks::stdout_color_sink_mt;
using StderrSink = spdlog::sinks::stderr_color_sink_mt;
using FileSink = spdlog::sinks::basic_file_sink_mt;
using RotatingFileSink = spdlog::sinks::rotating_file_sink_mt;
using DailyFileSink = spdlog::sinks::daily_file_sink_mt;
using CallbackSink = spdlog::sinks::callback_sink_mt;

// One sink owns a base path; share that sink when several loggers write to it.
// Generated files use <filename>.kzlog.<session>.<sink>.<sequence> names.
class ManagedFileSink final : public spdlog::sinks::base_sink<std::mutex> {
public:
  explicit ManagedFileSink(const FileSinkOptions& options);

  ~ManagedFileSink() override;

  auto current_file() -> std::filesystem::path;

  // Removes only closed regular files in this sink's naming namespace.
  auto cleanup() -> std::size_t;

  // Latest cleanup failure, background or explicit; cleared by a cleanup without errors.
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

} // namespace kitzoo::log

#endif // KITZOO_LOG_SINKS_HPP
