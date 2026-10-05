// ---------------------------------------------------------------------------
// kitzoo/string benchmarks
//
// Benchmarks that compare implementation strategies and measure overhead
// of string operations at different scales.
// ---------------------------------------------------------------------------

#include <kitzoo/string/string_utils.hpp>

#include <benchmark/benchmark.h>
#include <string>
#include <string_view>
#include <vector>

using namespace kitzoo::str;

// -- trim --------------------------------------------------------------------
// Measure trim on short strings (typical use case: configuration values).

static void BM_TrimShort(benchmark::State& state) {
  std::string const s = "  hello  ";
  for (auto _ : state) {
    auto v = trim(s);
    benchmark::DoNotOptimize(v);
  }
}

BENCHMARK(BM_TrimShort);

// Measure trim on long strings (pathological case).
static void BM_TrimLong(benchmark::State& state) {
  std::string const s = std::string(1000, ' ') + "hello" + std::string(1000, ' ');
  for (auto _ : state) {
    auto v = trim(s);
    benchmark::DoNotOptimize(v);
  }
}

BENCHMARK(BM_TrimLong);

// -- join --------------------------------------------------------------------
// Join with many small strings — tests allocation precision.

static void BM_JoinManySmall(benchmark::State& state) {
  int const n = static_cast<int>(state.range(0));
  std::vector<std::string_view> parts(static_cast<std::size_t>(n), "x");
  for (auto _ : state) {
    auto result = join(parts, ",");
    benchmark::DoNotOptimize(result);
  }
}

BENCHMARK(BM_JoinManySmall)->Arg(10)->Arg(100)->Arg(1000);

// -- replace_all -------------------------------------------------------------
// Measure performance with varying match density (no match, sparse, dense).

static void BM_ReplaceAllNoMatch(benchmark::State& state) {
  std::string const input(static_cast<std::size_t>(state.range(0)), 'x');
  for (auto _ : state) {
    auto result = replace_all(input, "y", "z");
    benchmark::DoNotOptimize(result);
  }
}

BENCHMARK(BM_ReplaceAllNoMatch)->Arg(100)->Arg(1000)->Arg(10000);

static void BM_ReplaceAllDense(benchmark::State& state) {
  int const n = static_cast<int>(state.range(0));
  std::string const input(static_cast<std::size_t>(n), 'x');
  for (auto _ : state) {
    auto result = replace_all(input, "x", "hello");
    benchmark::DoNotOptimize(result);
  }
}

BENCHMARK(BM_ReplaceAllDense)->Arg(10)->Arg(100)->Arg(1000);

// -- to_lower ----------------------------------------------------------------
// Lowercasing at various sizes.

static void BM_ToLower(benchmark::State& state) {
  std::string const input(static_cast<std::size_t>(state.range(0)), 'A');
  for (auto _ : state) {
    auto result = to_lower(input);
    benchmark::DoNotOptimize(result);
  }
}

BENCHMARK(BM_ToLower)->Arg(16)->Arg(256)->Arg(4096);
