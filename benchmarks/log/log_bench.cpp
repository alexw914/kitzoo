// ---------------------------------------------------------------------------
// Logging benchmarks: sync vs async producer-side overhead
//
// A NullSink discards everything so we measure logging pipeline cost
// (record construction, formatting, queueing) rather than I/O.
// ---------------------------------------------------------------------------

#include <kitzoo/log/async_logger.hpp>
#include <kitzoo/log/logger.hpp>

#include <benchmark/benchmark.h>
#include <memory>
#include <spdlog/sinks/base_sink.h>

using namespace kitzoo::log;

namespace {

class NullSink final : public spdlog::sinks::base_sink<std::mutex> {
protected:
  auto sink_it_(const spdlog::details::log_msg&) -> void override {}

  auto flush_() -> void override {}
};

} // namespace

static void BM_SyncLogOverhead(benchmark::State& state) {
  Logger logger{"bench"};
  logger.add_sink(std::make_shared<NullSink>());
  for (auto _ : state) {
    logger.log(kitzoo::log::Level::Info, "benchmark message");
  }
}

BENCHMARK(BM_SyncLogOverhead);

static void BM_SyncLogFilteredOut(benchmark::State& state) {
  Logger logger{"bench"};
  logger.add_sink(std::make_shared<NullSink>());
  logger.set_level(Level::Error);
  for (auto _ : state) {
    logger.log(kitzoo::log::Level::Debug, "filtered out — should be nearly free");
  }
}

BENCHMARK(BM_SyncLogFilteredOut);

static void BM_AsyncLogProducerOverhead(benchmark::State& state) {
  auto logger = std::make_shared<Logger>("bench-async");
  logger->add_sink(std::make_shared<NullSink>());
  AsyncLogger async{logger};
  for (auto _ : state) {
    async.log(kitzoo::log::Level::Info, "benchmark message");
  }
  // Destructor (after timing loop) drains the queue.
}

BENCHMARK(BM_AsyncLogProducerOverhead);

static void BM_AsyncLogContendedProducers(benchmark::State& state) {
  static std::unique_ptr<AsyncLogger> async;
  if (state.thread_index() == 0) {
    auto logger = std::make_shared<Logger>("bench-async-mp");
    logger->add_sink(std::make_shared<NullSink>());
    async = std::make_unique<AsyncLogger>(logger);
  }
  for (auto _ : state) {
    async->log(kitzoo::log::Level::Info, "benchmark message");
  }
  if (state.thread_index() == 0)
    async.reset();
}

BENCHMARK(BM_AsyncLogContendedProducers)->ThreadRange(1, 8)->UseRealTime();
