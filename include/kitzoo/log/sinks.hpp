// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: include/kitzoo/log/sinks.hpp
// Description: Declares console and callback sinks and the rolling file sink
//              used by loggers.
// -----------------------------------------------------------------------------

#ifndef KITZOO_LOG_SINKS_HPP
#define KITZOO_LOG_SINKS_HPP

#include <kitzoo/log/logger.hpp>

#include <cstddef>
#include <filesystem>
#include <mutex>
#include <spdlog/sinks/base_sink.h>
#include <spdlog/sinks/callback_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <string>

namespace kitzoo::log {

using ConsoleSink = spdlog::sinks::stdout_color_sink_mt;
using StderrSink = spdlog::sinks::stderr_color_sink_mt;
using CallbackSink = spdlog::sinks::callback_sink_mt;

// Rolls over by size and deletes expired files at startup and every
// cleanup_interval. Share one sink when several loggers write to the same path.
class RollingFileSink final : public spdlog::sinks::base_sink<std::mutex> {
public:
  explicit RollingFileSink(const FileSinkOptions& options);

  ~RollingFileSink() override;

  auto current_file() -> std::filesystem::path;

  // Deletes expired closed files of this sink's naming pattern; returns the count.
  auto cleanup() -> std::size_t;

  // Latest cleanup failure, background or explicit; cleared by a cleanup without errors.
  auto cleanup_error() -> std::string;

private:
  auto sink_it_(const spdlog::details::log_msg& message) -> void override;

  auto flush_() -> void override;

  auto open_file() -> void;

  auto cleanup_files() -> std::size_t;

  struct Impl;
  memory::UniquePtr<Impl> impl_;
};

} // namespace kitzoo::log

#endif // KITZOO_LOG_SINKS_HPP
