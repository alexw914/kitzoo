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

static void BM_SpscThroughput(benchmark::State& state) {
  for (auto _ : state) {
    state.PauseTiming();
    SPSCQueue<std::int64_t, 4096> q;
    std::atomic<bool> running{true};
    std::int64_t consumed = 0;

    std::jthread producer{[&] {
      std::int64_t i = 0;
      while (running.load(std::memory_order_relaxed)) {
        if (q.push(i))
          ++i;
      }
    }};
    std::jthread consumer{[&] {
      while (running.load(std::memory_order_relaxed) || !q.empty()) {
        if (q.pop().has_value())
          ++consumed;
      }
    }};

    state.ResumeTiming();
    // Let it run for the measured window; the loop body is the overhead.
    std::this_thread::sleep_for(std::chrono::milliseconds{10});
    state.PauseTiming();

    running.store(false, std::memory_order_relaxed);
    producer.join();
    consumer.join();
    state.ResumeTiming();
    state.SetItemsProcessed(consumed);
  }
}

BENCHMARK(BM_SpscThroughput)->UseRealTime();

// -- BlockingQueue throughput: 1 producer + 1 consumer -------------------------

static void BM_BlockingQueueThroughput(benchmark::State& state) {
  for (auto _ : state) {
    state.PauseTiming();
    BlockingQueue<std::int64_t> q;
    std::atomic<bool> running{true};
    std::int64_t consumed = 0;

    std::jthread producer{[&] {
      std::int64_t i = 0;
      while (running.load(std::memory_order_relaxed)) {
        q.push(i++);
      }
    }};
    std::jthread consumer{[&] {
      // wait_and_pop returns nullopt once the queue is closed+drained,
      // so the consumer terminates cleanly without busy-spinning.
      while (q.wait_and_pop().has_value()) {
        ++consumed;
      }
    }};

    state.ResumeTiming();
    std::this_thread::sleep_for(std::chrono::milliseconds{10});
    state.PauseTiming();

    running.store(false, std::memory_order_relaxed);
    producer.join();
    q.close(); // wake consumer; it drains and exits
    consumer.join();
    state.ResumeTiming();
    state.SetItemsProcessed(consumed);
  }
}

BENCHMARK(BM_BlockingQueueThroughput)->UseRealTime();
