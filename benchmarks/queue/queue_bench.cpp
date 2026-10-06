// ---------------------------------------------------------------------------
// Queue benchmarks: BlockingQueue vs SPSCQueue (latency + throughput)
// ---------------------------------------------------------------------------

#include <kitzoo/queue/blocking_queue.hpp>
#include <kitzoo/queue/concurrent_queue.hpp>
#include <kitzoo/queue/mpmc_queue.hpp>
#include <kitzoo/queue/spsc_queue.hpp>

#include <atomic>
#include <benchmark/benchmark.h>
#include <cstdint>
#include <thread>
#include <vector>

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

// -- MPMC throughput: N producers + N consumers --------------------------------

template <typename Push, typename Pop>
static void run_mpmc(benchmark::State& state, Push push, Pop pop) {
  const auto pairs = static_cast<int>(state.range(0));
  const std::int64_t per_producer = kThroughputItems / pairs;
  for (auto _ : state) {
    std::vector<std::jthread> threads;
    for (int p = 0; p < pairs; ++p) {
      threads.emplace_back([&] {
        for (std::int64_t i = 0; i < per_producer; ++i)
          push(i);
      });
      threads.emplace_back([&] {
        std::int64_t sum = 0;
        for (std::int64_t i = 0; i < per_producer; ++i)
          sum += pop();
        benchmark::DoNotOptimize(sum);
      });
    }
  }
  state.SetItemsProcessed(state.iterations() * per_producer * pairs);
}

static void BM_MpmcQueueThroughput(benchmark::State& state) {
  MPMCQueue<std::int64_t> q{4096};
  run_mpmc(
      state,
      [&](std::int64_t v) {
        while (!q.push(v)) {
        }
      },
      [&]() -> std::int64_t {
        for (;;)
          if (auto v = q.pop())
            return *v;
      });
}

BENCHMARK(BM_MpmcQueueThroughput)->Arg(1)->Arg(2)->Arg(4)->UseRealTime();

static void BM_BlockingQueueMpmcThroughput(benchmark::State& state) {
  BlockingQueue<std::int64_t> q{4096};
  run_mpmc(state, [&](std::int64_t v) { q.push(v); }, [&]() -> std::int64_t { return *q.wait_and_pop(); });
}

BENCHMARK(BM_BlockingQueueMpmcThroughput)->Arg(1)->Arg(2)->Arg(4)->UseRealTime();

static void BM_ConcurrentQueueMpmcThroughput(benchmark::State& state) {
  ConcurrentQueue<std::int64_t> q{4096};
  run_mpmc(
      state, [&](std::int64_t v) { q.enqueue(v); },
      [&]() -> std::int64_t {
        std::int64_t v = 0;
        while (!q.try_dequeue(v)) {
        }
        return v;
      });
}

BENCHMARK(BM_ConcurrentQueueMpmcThroughput)->Arg(1)->Arg(2)->Arg(4)->UseRealTime();
