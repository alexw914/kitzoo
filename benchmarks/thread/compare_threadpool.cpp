// ---------------------------------------------------------------------------
// Thread pool comparison: kitzoo ThreadPool vs BS::thread_pool (v5)
//
// Same workloads, same machine:
//   1. Submission latency: submit_task + wait for result (ping-pong)
//   2. Tiny-task throughput: 10k empty tasks
//   3. CPU-bound scaling: 1/2/4/8 workers on real work
//   4. Detached tiny tasks followed by wait()
//   5. Four external threads submitting detached tasks concurrently
// ---------------------------------------------------------------------------

#include <kitzoo/thread/thread_pool.hpp>

#include <BS_thread_pool.hpp>
#include <atomic>
#include <benchmark/benchmark.h>
#include <cstdint>
#include <thread>
#include <vector>

using namespace kitzoo;
using namespace kitzoo::thread;

// -- Submission latency ---------------------------------------------------------

static void BM_Latency_Kitzoo(benchmark::State& state) {
  ThreadPool pool{2};
  for (auto _ : state) {
    auto f = pool.submit_task([] { return 1; });
    benchmark::DoNotOptimize(f.get());
  }
}

BENCHMARK(BM_Latency_Kitzoo);

static void BM_Latency_BS(benchmark::State& state) {
  BS::thread_pool<> pool{2};
  for (auto _ : state) {
    auto f = pool.submit_task([] { return 1; });
    benchmark::DoNotOptimize(f.get());
  }
}

BENCHMARK(BM_Latency_BS);

// -- Tiny-task throughput -------------------------------------------------------

template <typename Submit>
static void tiny_task_throughput(benchmark::State& state, Submit&& submit) {
  constexpr int kTasks = 10000;
  for (auto _ : state) {
    std::atomic<int> counter{0};
    std::vector<std::future<void>> futures;
    futures.reserve(kTasks);
    for (int i = 0; i < kTasks; ++i) {
      futures.push_back(submit([&counter] { counter.fetch_add(1, std::memory_order_relaxed); }));
    }
    for (auto& f : futures)
      f.get();
    benchmark::DoNotOptimize(counter.load());
  }
  state.SetItemsProcessed(state.iterations() * kTasks);
}

static void BM_TinyTasks_Kitzoo(benchmark::State& state) {
  ThreadPool pool{static_cast<std::size_t>(state.range(0))};
  tiny_task_throughput(state, [&pool](auto&& fn) { return pool.submit_task(fn); });
}

BENCHMARK(BM_TinyTasks_Kitzoo)->Arg(1)->Arg(4)->Arg(8);

static void BM_TinyTasks_BS(benchmark::State& state) {
  BS::thread_pool<> pool{static_cast<std::size_t>(state.range(0))};
  tiny_task_throughput(state, [&pool](auto&& fn) { return pool.submit_task(fn); });
}

BENCHMARK(BM_TinyTasks_BS)->Arg(1)->Arg(4)->Arg(8);

// -- CPU-bound scaling ----------------------------------------------------------

static std::int64_t cpu_work(std::int64_t n) {
  std::int64_t acc = 0;
  for (std::int64_t i = 0; i < n; ++i)
    acc += (i * i) % 7;
  return acc;
}

static void BM_CpuBound_Kitzoo(benchmark::State& state) {
  ThreadPool pool{static_cast<std::size_t>(state.range(0))};
  constexpr std::int64_t kWork = 200'000;
  constexpr int kTasks = 16;
  for (auto _ : state) {
    std::vector<std::future<std::int64_t>> futures;
    futures.reserve(kTasks);
    for (int i = 0; i < kTasks; ++i)
      futures.push_back(pool.submit_task(cpu_work, kWork));
    std::int64_t total = 0;
    for (auto& f : futures)
      total += f.get();
    benchmark::DoNotOptimize(total);
  }
}

BENCHMARK(BM_CpuBound_Kitzoo)->Arg(1)->Arg(2)->Arg(4)->Arg(8)->UseRealTime();

static void BM_CpuBound_BS(benchmark::State& state) {
  BS::thread_pool<> pool{static_cast<std::size_t>(state.range(0))};
  constexpr std::int64_t kWork = 200'000;
  constexpr int kTasks = 16;
  for (auto _ : state) {
    std::vector<std::future<std::int64_t>> futures;
    futures.reserve(kTasks);
    for (int i = 0; i < kTasks; ++i)
      futures.push_back(pool.submit_task([] { return cpu_work(kWork); }));
    std::int64_t total = 0;
    for (auto& f : futures)
      total += f.get();
    benchmark::DoNotOptimize(total);
  }
}

BENCHMARK(BM_CpuBound_BS)->Arg(1)->Arg(2)->Arg(4)->Arg(8)->UseRealTime();

// -- Detached tiny tasks + wait --------------------------------------------------

template <typename Pool>
static void detach_tiny_tasks(benchmark::State& state, Pool& pool) {
  constexpr int kTasks = 10000;
  std::atomic<int> counter{0};
  for (auto _ : state) {
    for (int i = 0; i < kTasks; ++i)
      pool.detach_task([&counter] { counter.fetch_add(1, std::memory_order_relaxed); });
    pool.wait();
  }
  benchmark::DoNotOptimize(counter.load());
  state.SetItemsProcessed(state.iterations() * kTasks);
}

static void BM_DetachTasks_Kitzoo(benchmark::State& state) {
  ThreadPool pool{static_cast<std::size_t>(state.range(0))};
  detach_tiny_tasks(state, pool);
}

BENCHMARK(BM_DetachTasks_Kitzoo)->Arg(1)->Arg(4)->Arg(8)->UseRealTime();

static void BM_DetachTasks_BS(benchmark::State& state) {
  BS::thread_pool<> pool{static_cast<std::size_t>(state.range(0))};
  detach_tiny_tasks(state, pool);
}

BENCHMARK(BM_DetachTasks_BS)->Arg(1)->Arg(4)->Arg(8)->UseRealTime();

// -- Concurrent submitters ---------------------------------------------------------

template <typename Pool>
static void concurrent_submitters(benchmark::State& state, Pool& pool) {
  constexpr int kSubmitters = 4;
  constexpr int kTasksPerSubmitter = 2500;
  std::atomic<int> counter{0};
  for (auto _ : state) {
    {
      std::vector<std::jthread> submitters;
      for (int t = 0; t < kSubmitters; ++t)
        submitters.emplace_back([&] {
          for (int i = 0; i < kTasksPerSubmitter; ++i)
            pool.detach_task([&counter] { counter.fetch_add(1, std::memory_order_relaxed); });
        });
    }
    pool.wait();
  }
  benchmark::DoNotOptimize(counter.load());
  state.SetItemsProcessed(state.iterations() * kSubmitters * kTasksPerSubmitter);
}

static void BM_ConcurrentSubmit_Kitzoo(benchmark::State& state) {
  ThreadPool pool{static_cast<std::size_t>(state.range(0))};
  concurrent_submitters(state, pool);
}

BENCHMARK(BM_ConcurrentSubmit_Kitzoo)->Arg(4)->Arg(8)->UseRealTime();

static void BM_ConcurrentSubmit_BS(benchmark::State& state) {
  BS::thread_pool<> pool{static_cast<std::size_t>(state.range(0))};
  concurrent_submitters(state, pool);
}

BENCHMARK(BM_ConcurrentSubmit_BS)->Arg(4)->Arg(8)->UseRealTime();
