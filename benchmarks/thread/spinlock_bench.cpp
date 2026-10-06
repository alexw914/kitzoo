// ---------------------------------------------------------------------------
// SpinLock benchmarks: uncontended and contended critical sections
// ---------------------------------------------------------------------------

#include <kitzoo/thread/spinlock.hpp>

#include <benchmark/benchmark.h>
#include <cstdint>
#include <mutex>
#include <shared_mutex>

using namespace kitzoo::thread;

namespace {
SpinLock spin_lock;
RWSpinLock rw_lock;
std::int64_t counter = 0;
} // namespace

static void BM_SpinLockContended(benchmark::State& state) {
  for (auto _ : state) {
    std::lock_guard lock{spin_lock};
    benchmark::DoNotOptimize(++counter);
  }
}

BENCHMARK(BM_SpinLockContended)->ThreadRange(1, 8)->UseRealTime();

static void BM_RWSpinLockWriteContended(benchmark::State& state) {
  for (auto _ : state) {
    std::lock_guard lock{rw_lock};
    benchmark::DoNotOptimize(++counter);
  }
}

BENCHMARK(BM_RWSpinLockWriteContended)->ThreadRange(1, 8)->UseRealTime();

static void BM_RWSpinLockReadMostly(benchmark::State& state) {
  std::int64_t i = 0;
  for (auto _ : state) {
    if (++i % 16 == 0) {
      std::lock_guard lock{rw_lock};
      benchmark::DoNotOptimize(++counter);
    } else {
      std::shared_lock lock{rw_lock};
      benchmark::DoNotOptimize(counter);
    }
  }
}

BENCHMARK(BM_RWSpinLockReadMostly)->ThreadRange(1, 8)->UseRealTime();
