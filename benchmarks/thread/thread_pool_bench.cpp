// ---------------------------------------------------------------------------
// ThreadPool benchmarks: submission latency, throughput, scaling
// ---------------------------------------------------------------------------

#include <kitzoo/thread/thread_pool.hpp>

#include <atomic>
#include <benchmark/benchmark.h>
#include <cstdint>
#include <vector>

using namespace kitzoo;
using namespace kitzoo::thread;

// -- Submission latency: submit + get (ping-pong through the pool) ------------

static void BM_ThreadPoolSubmitLatency(benchmark::State& state) {
    ThreadPool pool{2};
    for (auto _ : state) {
        auto f = pool.submit_task([] { return 1; });
        benchmark::DoNotOptimize(f.get());
    }
}
BENCHMARK(BM_ThreadPoolSubmitLatency);

// -- Tiny-task throughput: many trivial tasks ---------------------------------

static void BM_ThreadPoolTinyTaskThroughput(benchmark::State& state) {
    auto const threads = static_cast<std::size_t>(state.range(0));
    ThreadPool pool{threads};
    constexpr int kTasks = 10000;

    for (auto _ : state) {
        std::atomic<int> counter{0};
        std::vector<std::future<void>> futures;
        futures.reserve(kTasks);
        for (int i = 0; i < kTasks; ++i) {
            futures.push_back(
                pool.submit_task([&counter] { counter.fetch_add(1, std::memory_order_relaxed); }));
        }
        for (auto& f : futures)
            f.get();
        benchmark::DoNotOptimize(counter.load());
    }
    state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()) * kTasks);
}
BENCHMARK(BM_ThreadPoolTinyTaskThroughput)->Arg(1)->Arg(2)->Arg(4)->Arg(8);

// -- CPU-bound scaling: work that benefits from parallelism -------------------

static void BM_ThreadPoolCpuBound(benchmark::State& state) {
    auto const threads = static_cast<std::size_t>(state.range(0));
    ThreadPool pool{threads};

    auto work = [](std::int64_t n) {
        std::int64_t acc = 0;
        for (std::int64_t i = 0; i < n; ++i)
            acc += (i * i) % 7;
        return acc;
    };

    constexpr std::int64_t kWorkPerTask = 200'000;
    constexpr int kTasks = 16;

    for (auto _ : state) {
        std::vector<std::future<std::int64_t>> futures;
        futures.reserve(kTasks);
        for (int i = 0; i < kTasks; ++i)
            futures.push_back(pool.submit_task(work, kWorkPerTask));
        std::int64_t total = 0;
        for (auto& f : futures)
            total += f.get();
        benchmark::DoNotOptimize(total);
    }
}
BENCHMARK(BM_ThreadPoolCpuBound)->Arg(1)->Arg(2)->Arg(4)->Arg(8)->UseRealTime();
