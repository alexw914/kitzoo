// ---------------------------------------------------------------------------
// Queue benchmarks: BlockingQueue vs SPSCQueue (latency + throughput)
// ---------------------------------------------------------------------------

#include <kitzoo/queue/blocking_queue.hpp>
#include <kitzoo/queue/spsc_queue.hpp>

#include <atomic>
#include <benchmark/benchmark.h>
#include <cstdint>
#include <thread>

using namespace kitzoo;
using namespace kitzoo::queue;

// -- Single-threaded push/pop (latency baseline) -------------------------------

static void BM_SpscPushPopSingleThread(benchmark::State& state) {
  SPSCQueue<std::int64_t, 1024> q;
  std::int64_t counter = 0;
  for (auto _ : state) {
    benchmark::DoNotOptimize(q.push(counter));
    benchmark::DoNotOptimize(q.pop());
    ++counter;
  }
}

BENCHMARK(BM_SpscPushPopSingleThread);

static void BM_BlockingQueuePushPopSingle(benchmark::State& state) {
  BlockingQueue<std::int64_t> q;
  std::int64_t counter = 0;
  for (auto _ : state) {
    q.push(counter);
    benchmark::DoNotOptimize(q.wait_and_pop());
    ++counter;
  }
}

BENCHMARK(BM_BlockingQueuePushPopSingle);

// -- SPSC throughput: 1 producer + 1 consumer ---------------------------------

constexpr std::int64_t kThroughputItems = 1 << 20;

static void BM_SpscThroughput(benchmark::State& state) {
  for (auto _ : state) {
    SPSCQueue<std::int64_t, 4096> q;
    std::jthread producer{[&] {
      for (std::int64_t i = 0; i < kThroughputItems;)
        if (q.push(i))
          ++i;
    }};
    std::int64_t sum = 0;
    for (std::int64_t n = 0; n < kThroughputItems;) {
      if (auto v = q.pop()) {
        sum += *v;
        ++n;
      }
    }
    benchmark::DoNotOptimize(sum);
  }
  state.SetItemsProcessed(state.iterations() * kThroughputItems);
}

BENCHMARK(BM_SpscThroughput)->UseRealTime();

// -- BlockingQueue throughput: 1 producer + 1 consumer -------------------------

static void BM_BlockingQueueThroughput(benchmark::State& state) {
  for (auto _ : state) {
    BlockingQueue<std::int64_t> q{4096};
    std::jthread producer{[&] {
      for (std::int64_t i = 0; i < kThroughputItems; ++i)
        q.push(i);
    }};
    std::int64_t sum = 0;
    for (std::int64_t n = 0; n < kThroughputItems; ++n)
      sum += *q.wait_and_pop();
    benchmark::DoNotOptimize(sum);
  }
  state.SetItemsProcessed(state.iterations() * kThroughputItems);
}

BENCHMARK(BM_BlockingQueueThroughput)->UseRealTime();
