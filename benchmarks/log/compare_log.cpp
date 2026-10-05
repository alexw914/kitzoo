// ---------------------------------------------------------------------------
// Logging comparison: kitzoo sync/async Logger vs spdlog (v1.17)
//
// Fairness notes:
//   - Both sides format a full record (timestamp, level, thread id, name) and
//     write to a sink that discards output — we measure the logging pipeline,
//     not I/O.
//   - Both sides format timestamp, level, thread id, logger, source and message.
//   - spdlog async uses its internal bounded queue + worker thread;
//     kitzoo AsyncLogger uses BlockingQueue + jthread worker.
// ---------------------------------------------------------------------------

#include <kitzoo/log/async_logger.hpp>
#include <kitzoo/log/logger.hpp>

#include <benchmark/benchmark.h>
#include <chrono>
#include <memory>
#include <mutex>
#include <spdlog/async.h>
#include <spdlog/sinks/base_sink.h>
#include <spdlog/spdlog.h>

using namespace kitzoo::log;

namespace {

constexpr auto kAsyncBatchSize = 10000;
constexpr auto kAsyncMessage = "the quick brown fox jumps over the lazy dog";

class NullSink final : public spdlog::sinks::base_sink<std::mutex> {
protected:
  void sink_it_(spdlog::details::log_msg const&) override {}

  void flush_() override {}
};

} // namespace

// -- Sync logging overhead ------------------------------------------------------

static void BM_Sync_Kitzoo(benchmark::State& state) {
  Logger logger{"bench"};
  logger.add_sink(std::make_shared<NullSink>());
  for (auto _ : state) {
    logger.log(Level::Info, "the quick brown fox jumps over the lazy dog");
  }
}

BENCHMARK(BM_Sync_Kitzoo);

static void BM_Sync_Spdlog(benchmark::State& state) {
  auto logger = spdlog::logger{"bench", std::make_shared<NullSink>()};
  logger.set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [tid=%t] [%n] %g:%# %v");
  for (auto _ : state) {
    logger.info("the quick brown fox jumps over the lazy dog");
  }
}

BENCHMARK(BM_Sync_Spdlog);

// -- Level-filtered (should be nearly free on both sides) -----------------------

static void BM_Filtered_Kitzoo(benchmark::State& state) {
  Logger logger{"bench"};
  logger.add_sink(std::make_shared<NullSink>());
  logger.set_level(Level::Error);
  for (auto _ : state) {
    logger.log(Level::Debug, "filtered out");
  }
}

BENCHMARK(BM_Filtered_Kitzoo);

static void BM_Filtered_Spdlog(benchmark::State& state) {
  auto logger = spdlog::logger{"bench", std::make_shared<NullSink>()};
  logger.set_level(spdlog::level::err);
  for (auto _ : state) {
    logger.debug("filtered out");
  }
}

BENCHMARK(BM_Filtered_Spdlog);

// -- Async producer-side overhead ------------------------------------------------

static void BM_Async_Kitzoo(benchmark::State& state) {
  auto logger = std::make_shared<Logger>("bench-async");
  logger->add_sink(std::make_shared<NullSink>());
  auto async = std::make_unique<AsyncLogger>(logger);
  for (auto _ : state) {
    auto const start = std::chrono::steady_clock::now();
    for (auto i = 0; i < kAsyncBatchSize; ++i)
      async->log(Level::Info, kAsyncMessage);
    auto const elapsed = std::chrono::steady_clock::now() - start;
    state.SetIterationTime(std::chrono::duration<double>(elapsed).count());
  }
  state.SetItemsProcessed(kAsyncBatchSize);
  async.reset(); // drain outside the timed producer loop
}

BENCHMARK(BM_Async_Kitzoo)->Iterations(1)->UseManualTime();

static void BM_Async_Spdlog(benchmark::State& state) {
  spdlog::init_thread_pool(65536, 1);
  auto async = std::make_shared<spdlog::async_logger>("bench-async", std::make_shared<NullSink>(),
                                                      spdlog::thread_pool(), spdlog::async_overflow_policy::block);
  for (auto _ : state) {
    auto const start = std::chrono::steady_clock::now();
    for (auto i = 0; i < kAsyncBatchSize; ++i)
      async->info(kAsyncMessage);
    auto const elapsed = std::chrono::steady_clock::now() - start;
    state.SetIterationTime(std::chrono::duration<double>(elapsed).count());
  }
  state.SetItemsProcessed(kAsyncBatchSize);
  async.reset();
  spdlog::shutdown(); // drain and join outside the timed producer loop
}

BENCHMARK(BM_Async_Spdlog)->Iterations(1)->UseManualTime();
