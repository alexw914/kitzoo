// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: tests/time/time_watcher_test.cpp
// Description: Verifies named measurements, scope ownership and callback completion.
// -----------------------------------------------------------------------------

#include <kitzoo/time/time_watcher.hpp>

#include <atomic>
#include <gtest/gtest.h>
#include <optional>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

namespace {
using namespace kitzoo::time;
using namespace std::chrono_literals;

TEST(TimeWatcherTest, NamedMeasurementsCacheResultsAndCanRestart) {
  TimeWatcher watcher;
  EXPECT_FALSE(watcher.last_result("decode"));
  EXPECT_THROW(watcher.end("decode"), std::out_of_range);
  watcher.begin("decode");
  EXPECT_THROW(watcher.begin("decode"), std::logic_error);
  const auto first = watcher.end("decode");
  EXPECT_GE(first, 0ns);
  EXPECT_EQ(watcher.last_result("decode"), first);
  EXPECT_THROW(watcher.end("decode"), std::out_of_range);
  watcher.begin("decode");
  EXPECT_EQ(watcher.last_result("decode"), first);
  const auto second = watcher.end("decode");
  EXPECT_EQ(watcher.last_result("decode"), second);
}

TEST(TimeWatcherTest, ScopeCachesResultBeforeReentrantCallback) {
  TimeWatcher watcher;
  int calls = 0;
  TimeDuration result{};
  {
    auto scope = watcher.scope("frame", [&](std::string_view name, TimeDuration elapsed) {
      ++calls;
      result = elapsed;
      EXPECT_EQ(name, "frame");
      EXPECT_EQ(watcher.last_result(name), elapsed);
      watcher.begin("callback");
      (void)watcher.end("callback");
    });
  }
  EXPECT_EQ(calls, 1);
  EXPECT_GE(result, 0ns);
  EXPECT_EQ(watcher.last_result("frame"), result);
  EXPECT_TRUE(watcher.last_result("callback"));
}

TEST(TimeWatcherTest, ExplicitFinishAndMoveCompleteOnlyOnce) {
  TimeWatcher watcher;
  int calls = 0;
  {
    auto original = watcher.scope("frame", [&](std::string_view, TimeDuration) { ++calls; });
    auto moved = std::move(original);
    const auto elapsed = moved.finish();
    EXPECT_EQ(moved.finish(), elapsed);
    (void)original.finish();
    EXPECT_EQ(watcher.last_result("frame"), elapsed);
  }
  EXPECT_EQ(calls, 1);
}

TEST(TimeWatcherTest, MoveAssignmentFinishesDestinationBeforeTakingSource) {
  TimeWatcher watcher;
  int first_calls = 0;
  int second_calls = 0;
  {
    auto first = watcher.scope("first", [&](std::string_view, TimeDuration) { ++first_calls; });
    auto second = watcher.scope("second", [&](std::string_view, TimeDuration) { ++second_calls; });
    first = std::move(second);
    EXPECT_EQ(first_calls, 1);
    EXPECT_EQ(second_calls, 0);
    EXPECT_TRUE(watcher.last_result("first"));
    (void)first.finish();
  }
  EXPECT_EQ(first_calls, 1);
  EXPECT_EQ(second_calls, 1);
  EXPECT_TRUE(watcher.last_result("second"));
}

TEST(TimeWatcherTest, ExplicitCallbackExceptionRetainsResultAndIsNotRepeated) {
  TimeWatcher watcher;
  int calls = 0;
  auto scope = watcher.scope("failure", [&](std::string_view, TimeDuration) {
    ++calls;
    throw std::runtime_error("callback failed");
  });
  EXPECT_THROW(scope.finish(), std::runtime_error);
  const auto cached = watcher.last_result("failure");
  ASSERT_TRUE(cached);
  EXPECT_EQ(scope.finish(), *cached);
  EXPECT_EQ(calls, 1);
}

TEST(TimeWatcherTest, DestructorCompletesDuringUnwind) {
  TimeWatcher watcher;
  int calls = 0;
  try {
    auto scope = watcher.scope("unwind", [&](std::string_view, TimeDuration) { ++calls; });
    throw std::logic_error("work failed");
  } catch (const std::logic_error& error) {
    EXPECT_STREQ(error.what(), "work failed");
  }
  EXPECT_EQ(calls, 1);
  EXPECT_TRUE(watcher.last_result("unwind"));
}

TEST(TimeWatcherTest, ThrowingCallbackInDestructorTerminates) {
  EXPECT_DEATH(
      {
        TimeWatcher watcher;
        auto scope =
            watcher.scope("fatal", [](std::string_view, TimeDuration) { throw std::runtime_error("callback failed"); });
      },
      "");
}

TEST(TimeWatcherTest, ScopeCanOutliveWatcher) {
  std::optional<TimeWatcher::Scope> scope;
  int calls = 0;
  {
    TimeWatcher watcher;
    scope.emplace(watcher.scope("late", [&](std::string_view, TimeDuration elapsed) {
      ++calls;
      EXPECT_GE(elapsed, 0ns);
    }));
  }
  scope.reset();
  EXPECT_EQ(calls, 1);
}

TEST(TimeWatcherTest, ConcurrentIndependentNamesCompleteEveryScope) {
  TimeWatcher watcher;
  std::atomic<int> completed{0};
  std::vector<std::jthread> workers;
  for (int worker = 0; worker < 4; ++worker)
    workers.emplace_back([&, worker] {
      const auto name = "worker_" + std::to_string(worker);
      for (int i = 0; i < 50; ++i) {
        watcher.begin(name);
        (void)watcher.end(name);
        auto scope = watcher.scope(name, [&](std::string_view, TimeDuration) { ++completed; });
      }
    });
  workers.clear();
  EXPECT_EQ(completed.load(), 200);
  for (int worker = 0; worker < 4; ++worker)
    EXPECT_TRUE(watcher.last_result("worker_" + std::to_string(worker)));
}
} // namespace
