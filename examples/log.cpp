// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: examples/log.cpp
// Description: Demonstrates synchronous, callback, file and asynchronous logging.
// -----------------------------------------------------------------------------

#include <kitzoo/log.hpp>
#include <kitzoo/os/filesys.hpp>

#include <chrono>
#include <iostream>
#include <memory>
#include <source_location>
#include <string>
#include <thread>
#include <vector>

namespace {
using namespace kitzoo::log;

auto synchronous_logging() -> void {
  // Macros use the process-wide logger and skip disabled argument evaluation.
  KZ_LOG_INFO("application started: version={}", "0.1");

  LoggerOptions options;
  options.pattern = "[%n][%l] %v";
  options.sinks = {std::make_shared<ConsoleSink>()};
  options.flush_level = Level::Error;
  Logger logger("capture", options);
  logger.logf(Level::Info, std::source_location::current(), "frame={} latency={:.2f}ms", 7, 1.25);
  logger.set_level(Level::Warn);
  logger.logf(Level::Debug, std::source_location::current(), "filtered frame={}", 8);
  logger.log(Level::Error, "capture failed; retrying"); // also flushes sinks
  logger.flush();
}

auto callback_and_historical_logging() -> void {
  LoggerOptions options;
  options.sinks = {std::make_shared<CallbackSink>([](const auto& record) {
    // The payload view only lives during this callback. Copy it when storing.
    std::cout << "callback: " << std::string(record.payload.data(), record.payload.size()) << '\n';
  })};
  options.error_handler = [](std::string_view error) {
    // Report errors through an independent channel, not this logger.
    std::cerr << "logging error: " << error << '\n';
  };
  Logger logger("events", options);
  const auto captured_at = std::chrono::system_clock::now() - std::chrono::seconds(2);
  logger.log_at(captured_at, Level::Info, "historical frame received");
  logger.logf(Level::Info, std::source_location::current(), "inference result: objects={}", 3);
}

// File logging: switch to new files by size and periodically clean closed files.
auto rolling_file_logging() -> void {
  const auto directory = kitzoo::os::temp_directory();
  std::filesystem::path latest;
  {
    Logger logger("file");
    logger.set_pattern("%v");
    FileSinkOptions file;
    file.path = directory / "application.log";
    file.max_size_bytes = 256;                     // Start a new file beyond 256 bytes.
    file.max_age = std::chrono::days{7};           // Delete files older than a week...
    file.cleanup_interval = std::chrono::hours{1}; // ...checking every hour.
    auto sink = logger.add_file_sink(file);
    for (int frame = 0; frame < 20; ++frame)
      logger.logf(Level::Info, std::source_location::current(), "frame={} status=processed", frame);
    logger.flush(); // Flush is not an fsync durability guarantee.
    latest = sink->current_file();
    std::cout << "manual cleanup removed " << sink->cleanup() << " files\n";
  } // Stops cleanup and releases file handles before removing the directory.
  std::cout << "latest log file: " << latest.filename().string() << '\n' << kitzoo::os::read_file(latest);
  kitzoo::os::remove_path(directory, true);
}

auto asynchronous_logging() -> void {
  LoggerOptions options;
  options.pattern = "[%n][%l] %v";
  options.sinks = {std::make_shared<ConsoleSink>()};
  // Reuse memory's factory; logging-owned strings and queue use MiAllocator too.
  auto logger = kitzoo::memory::make_shared<Logger>("workers", options);
  // Block preserves accepted records with a bounded queue. DropNewest avoids
  // waiting when full; inspect dropped_count() when selecting that policy.
  AsyncLogger async(logger, {.capacity = 64, .overflow_policy = OverflowPolicy::Block});
  std::vector<std::jthread> producers;
  for (int worker = 0; worker < 2; ++worker)
    producers.emplace_back([&, worker] {
      for (int frame = 0; frame < 3; ++frame)
        async.logf(Level::Info, std::source_location::current(), "worker={} frame={}", worker, frame);
    });
  producers.clear(); // producers finish before waiting for their records
  async.flush();     // drain prior records while keeping the logger open
  async.log(Level::Info, "all frames processed");
  async.close(); // stop acceptance, drain, join and flush; safe to repeat
  std::cout << "dropped=" << async.dropped_count() << " rejected=" << async.rejected_count()
            << " failed=" << logger->failed_count() << '\n';
}
} // namespace

auto main() -> int {
  synchronous_logging();
  callback_and_historical_logging();
  rolling_file_logging();
  asynchronous_logging();
}
