// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: tests/time/timer_test.cpp
// Description: Verifies periodic callbacks, restart, exceptions and shutdown.
// -----------------------------------------------------------------------------

#include <kitzoo/time/timer.hpp>

#include <atomic>
#include <future>
#include <gtest/gtest.h>
#include <memory>
#include <stdexcept>
#include <thread>

namespace {
using kitzoo::time::Timer;
using namespace std::chrono_literals;

TEST(TimerTest, RejectsNonPositiveIntervals) {
  EXPECT_THROW(Timer{0ms}, std::invalid_argument);
  EXPECT_THROW(Timer{-1ms}, std::invalid_argument);
}

TEST(TimerTest, CallbackFiresAndStopIsIdempotent) {
  Timer timer(1ms);
  EXPECT_FALSE(timer.running());
  std::promise<void> fired;
  auto ready = fired.get_future();
  std::atomic<int> calls{0};
  timer.start([&] {
    if (calls.fetch_add(1) == 0)
      fired.set_value();
  });
  EXPECT_TRUE(timer.running());
  const auto status = ready.wait_for(5s);
  timer.stop();
  EXPECT_EQ(status, std::future_status::ready);
  const int stopped_at = calls.load();
  timer.stop();
  EXPECT_FALSE(timer.running());
  EXPECT_GE(stopped_at, 1);
  EXPECT_EQ(calls.load(), stopped_at);
}

TEST(TimerTest, StopInterruptsLongIntervalWithoutCallingCallback) {
  Timer timer(1h);
  std::atomic<int> calls{0};
  timer.start([&] { ++calls; });
  const auto begin = std::chrono::steady_clock::now();
  timer.stop();
  EXPECT_LT(std::chrono::steady_clock::now() - begin, 5s);
  EXPECT_EQ(calls.load(), 0);
  EXPECT_FALSE(timer.running());
}

TEST(TimerTest, StartWhileRunningDoesNotReplaceCallback) {
  Timer timer(1ms);
  std::promise<void> fired;
  auto ready = fired.get_future();
  std::atomic<int> first_calls{0};
  std::atomic<int> second_calls{0};
  timer.start([&] {
    if (first_calls.fetch_add(1) == 0)
      fired.set_value();
  });
  timer.start([&] { ++second_calls; });
  const auto status = ready.wait_for(5s);
  timer.stop();
  EXPECT_EQ(status, std::future_status::ready);
  EXPECT_GE(first_calls.load(), 1);
  EXPECT_EQ(second_calls.load(), 0);
}

TEST(TimerTest, CanRestartWithNewCallbackAfterStop) {
  Timer timer(1ms);
  for (int cycle = 0; cycle < 2; ++cycle) {
    std::promise<void> fired;
    auto ready = fired.get_future();
    std::atomic<int> calls{0};
    timer.start([&] {
      if (calls.fetch_add(1) == 0)
        fired.set_value();
    });
    const auto status = ready.wait_for(5s);
    timer.stop(); // join before callback captures leave scope
    EXPECT_EQ(status, std::future_status::ready);
    EXPECT_GE(calls.load(), 1);
  }
}

TEST(TimerTest, CallbackExceptionsDoNotStopLaterTicks) {
  Timer timer(1ms);
  std::promise<void> continued;
  auto ready = continued.get_future();
  std::atomic<int> calls{0};
  timer.start([&] {
    const int call = calls.fetch_add(1);
    if (call == 0)
      throw std::runtime_error("first tick failed");
    if (call == 1)
      continued.set_value();
  });
  const auto status = ready.wait_for(5s);
  timer.stop();
  EXPECT_EQ(status, std::future_status::ready);
  EXPECT_GE(calls.load(), 2);
}

TEST(TimerTest, CallbackCanStopTimerAndOwnerCanRestart) {
  Timer timer(1ms);
  std::promise<void> stopped;
  auto ready = stopped.get_future();
  std::atomic<int> calls{0};
  timer.start([&] {
    if (calls.fetch_add(1) == 0) {
      timer.stop();
      stopped.set_value();
    }
  });
  ASSERT_EQ(ready.wait_for(5s), std::future_status::ready);
  EXPECT_FALSE(timer.running());
  std::this_thread::sleep_for(20ms);
  EXPECT_EQ(calls.load(), 1);

  std::promise<void> restarted;
  auto again = restarted.get_future();
  std::atomic<bool> fired{false};
  timer.start([&] {
    if (!fired.exchange(true))
      restarted.set_value();
  });
  EXPECT_EQ(again.wait_for(5s), std::future_status::ready);
  EXPECT_TRUE(timer.running());
  timer.stop();
}

TEST(TimerTest, AcceptsMoveOnlyCallbackAndDestructorStopsWorker) {
  std::promise<int> value;
  auto ready = value.get_future();
  {
    Timer timer(1ms);
    timer.start([owned = std::make_unique<int>(42), &value]() mutable {
      if (owned) {
        value.set_value(*owned);
        owned.reset();
      }
    });
    EXPECT_EQ(ready.wait_for(5s), std::future_status::ready);
  }
  ASSERT_EQ(ready.wait_for(0s), std::future_status::ready);
  EXPECT_EQ(ready.get(), 42);
}
} // namespace
