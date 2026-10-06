// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: tests/time/timeline_test.cpp
// Description: Verifies clock domains, replay progress, cancellation and selection.
// -----------------------------------------------------------------------------

#include <kitzoo/time/time.hpp>
#include <kitzoo/time/timeline.hpp>

#include <atomic>
#include <cstdlib>
#include <future>
#include <gtest/gtest.h>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {
using namespace kitzoo::time;
using namespace std::chrono_literals;

class CustomTimeline final : public Timeline {
public:
  auto timestamp(std::string_view key = {}) const -> TimeDuration override { return key == "frame" ? 42ns : 7ns; }

  auto is_valid() const -> bool override { return true; }
};

TEST(TimelineTest, SystemTimelineUsesUnixUtcDomain) {
  SystemTimeline system;
  const auto before = utc_timestamp().time_since_epoch();
  const auto reading = system.timestamp();
  EXPECT_GE(reading, before);
  EXPECT_LE(reading, utc_timestamp().time_since_epoch());
  EXPECT_TRUE(system.is_valid());
  EXPECT_EQ(system.type(), TimelineType::System);
}

TEST(TimelineTest, FeederRequiresSampleAndAcceptsZeroAndBackwardJumps) {
  FeederTimeline feeder;
  EXPECT_FALSE(feeder.is_valid());
  EXPECT_FALSE(feeder.sleep_for(1ms, 1ms));
  feeder.feed(0ns);
  EXPECT_TRUE(feeder.is_valid());
  EXPECT_EQ(feeder.type(), TimelineType::Feeder);
  EXPECT_EQ(feeder.timestamp(), 0ns);
  feeder.feed(100ns);
  feeder.feed(-1ns);
  EXPECT_EQ(feeder.timestamp(), -1ns);
}

TEST(TimelineTest, CallbackReceivesKeyAndPropagatesExceptions) {
  CallbackTimeline callback([](std::string_view key) -> TimeDuration {
    if (key == "invalid")
      throw std::runtime_error("clock failed");
    return key == "camera" ? 42ms : 7ms;
  });
  EXPECT_TRUE(callback.is_valid());
  EXPECT_EQ(callback.type(), TimelineType::Callback);
  EXPECT_EQ(callback.timestamp("camera"), 42ms);
  EXPECT_EQ(callback.timestamp(), 7ms);
  EXPECT_THROW(callback.timestamp("invalid"), std::runtime_error);
  EXPECT_THROW(CallbackTimeline(ClockCallback{}), std::invalid_argument);
  CustomTimeline custom;
  EXPECT_EQ(custom.type(), TimelineType::Custom);
  EXPECT_EQ(custom.timestamp("frame"), 42ns);
}

TEST(TimelineTest, OffsetForwardsKeysAndLeavesSourceUnchanged) {
  auto source = std::make_shared<CallbackTimeline>(
      [](std::string_view key) -> TimeDuration { return key == "camera" ? 20ms : 10ms; });
  OffsetTimeline offset(source, -2ms);
  EXPECT_EQ(offset.type(), TimelineType::Offset);
  EXPECT_TRUE(offset.is_valid());
  EXPECT_EQ(offset.timestamp("camera"), 18ms);
  EXPECT_EQ(source->timestamp("camera"), 20ms);
  offset.set_offset(3ms);
  EXPECT_EQ(offset.timestamp(), 13ms);
  EXPECT_THROW(OffsetTimeline(nullptr), std::invalid_argument);
}

TEST(TimelineTest, OffsetPreservesValidityAndRejectsArithmeticOverflow) {
  auto source = std::make_shared<FeederTimeline>();
  OffsetTimeline offset(source, 1ns);
  EXPECT_FALSE(offset.is_valid());
  source->feed(TimeDuration::max());
  EXPECT_TRUE(offset.is_valid());
  EXPECT_THROW(offset.timestamp(), std::overflow_error);
  source->feed(TimeDuration::min());
  offset.set_offset(-1ns);
  EXPECT_THROW(offset.timestamp(), std::overflow_error);
  offset.set_offset(0ns);
  EXPECT_EQ(offset.timestamp(), TimeDuration::min());
}

TEST(TimelineTest, AbsoluteWaitCompletesWhenProducerFeedsTarget) {
  FeederTimeline feeder;
  feeder.feed(0ns);
  std::promise<void> started;
  std::promise<bool> completed;
  auto result = completed.get_future();
  std::jthread waiter([&] {
    started.set_value();
    completed.set_value(feeder.sleep_until(10ms, 2s));
  });
  started.get_future().wait();
  feeder.feed(10ms); // absolute target avoids a race with relative wait setup
  EXPECT_EQ(result.wait_for(5s), std::future_status::ready);
  waiter.join();
  EXPECT_TRUE(result.get());
}

TEST(TimelineTest, BackwardJumpDoesNotPrematurelyCompleteWait) {
  std::atomic<int> samples{0};
  CallbackTimeline timeline([&](std::string_view) -> TimeDuration {
    const int sample = samples.fetch_add(1);
    return sample == 0 ? 10ms : sample == 1 ? -5ms : 20ms;
  });
  EXPECT_TRUE(timeline.sleep_until(20ms, 2s));
  EXPECT_GE(samples.load(), 3);
}

TEST(TimelineTest, RelativeWaitUsesTimelineProgress) {
  std::atomic<int> samples{0};
  CallbackTimeline timeline(
      [&](std::string_view) -> TimeDuration { return std::chrono::milliseconds(samples.fetch_add(1) * 5); });
  EXPECT_TRUE(timeline.sleep_for(10ms, 2s));
  EXPECT_GE(samples.load(), 3);
}

TEST(TimelineTest, PausedReplayTimesOutUsingSteadyClock) {
  FeederTimeline feeder;
  feeder.feed(0ns);
  const auto begin = std::chrono::steady_clock::now();
  EXPECT_FALSE(feeder.sleep_for(1s, 5ms));
  EXPECT_GE(std::chrono::steady_clock::now() - begin, 5ms);
  EXPECT_EQ(feeder.timestamp(), 0ns);
  EXPECT_TRUE(feeder.sleep_for(0ns));
  EXPECT_TRUE(feeder.sleep_for(-1ns));
}

TEST(TimelineTest, StopTokenCancelsUnlimitedWait) {
  FeederTimeline feeder;
  feeder.feed(0ns);
  std::promise<void> started;
  bool reached = true;
  std::jthread waiter([&](std::stop_token token) {
    started.set_value();
    reached = feeder.sleep_until(1s, {}, token);
  });
  started.get_future().wait();
  waiter.request_stop();
  waiter.join();
  EXPECT_FALSE(reached);
  std::stop_source stopped;
  stopped.request_stop();
  EXPECT_FALSE(feeder.sleep_for(1s, {}, stopped.get_token()));
}

// Reports itself as the system clock and counts how often sleeping checks it.
class CountingSystemTimeline final : public Timeline {
public:
  auto timestamp(std::string_view) const -> TimeDuration override {
    ++checks;
    return utc_timestamp().time_since_epoch();
  }

  auto is_valid() const -> bool override { return true; }

  auto type() const noexcept -> TimelineType override { return TimelineType::System; }

  mutable std::atomic<int> checks{0};
};

TEST(TimelineTest, SystemSleepWaitsForRemainingGapInsteadOfPolling) {
  CountingSystemTimeline timeline;
  const auto started = std::chrono::steady_clock::now();
  EXPECT_TRUE(timeline.sleep_for(200ms));
  EXPECT_GE(std::chrono::steady_clock::now() - started, 200ms);
  // Millisecond polling would check about 200 times; gap sleeps need a handful.
  EXPECT_LE(timeline.checks.load(), 10);
}

TEST(TimelineTest, OffsetOverSystemSleepsForRemainingGapInsteadOfPolling) {
  auto source = std::make_shared<CountingSystemTimeline>();
  OffsetTimeline timeline{source, 5s};
  const auto started = std::chrono::steady_clock::now();
  EXPECT_TRUE(timeline.sleep_for(200ms));
  EXPECT_GE(std::chrono::steady_clock::now() - started, 200ms);
  EXPECT_LE(source->checks.load(), 10);
}

TEST(TimelineTest, FeederWaitWakesWhenFedTimestampReachesTarget) {
  FeederTimeline feeder;
  feeder.feed(0ns);
  std::atomic<bool> done{false};
  std::jthread waiter([&] {
    EXPECT_TRUE(feeder.sleep_until(100ns));
    done = true;
  });
  std::this_thread::sleep_for(20ms);
  feeder.feed(50ns);
  std::this_thread::sleep_for(20ms);
  EXPECT_FALSE(done.load());
  feeder.feed(100ns);
  waiter.join();
  EXPECT_TRUE(done.load());
}

TEST(TimelineTest, OffsetWaitForwardsToFeederSource) {
  auto feeder = std::make_shared<FeederTimeline>();
  feeder->feed(0ns);
  OffsetTimeline timeline{feeder, 10ns};
  std::jthread waiter([&] { EXPECT_TRUE(timeline.sleep_until(110ns, 5s)); });
  std::this_thread::sleep_for(20ms);
  feeder->feed(100ns);
}

TEST(TimelineTest, RejectsNegativeTimeoutAndRelativeTargetOverflow) {
  FeederTimeline feeder;
  feeder.feed(TimeDuration::max());
  EXPECT_THROW(feeder.sleep_for(1ns), std::overflow_error);
  EXPECT_THROW(feeder.sleep_for(1s, -1ns), std::invalid_argument);
  EXPECT_THROW(feeder.sleep_until(1s, -1ns), std::invalid_argument);
}

// One-shot global selection is checked in isolated child processes.
auto verify_explicit_selection() -> void {
  auto& service = Time::instance();
  // Static setup also permits --gtest_repeat without trying to reset the singleton.
  static const bool initialized =
      service.init(std::make_shared<CallbackTimeline>([](std::string_view key) -> TimeDuration {
        EXPECT_EQ(Time::instance().timeline_type(), TimelineType::Callback);
        return key == "camera" ? 42ms : 0ns;
      }));
  EXPECT_TRUE(initialized);
  EXPECT_FALSE(service.init(nullptr));
  EXPECT_FALSE(service.init(std::make_shared<SystemTimeline>()));
  EXPECT_TRUE(service.is_valid());
  EXPECT_EQ(service.timestamp("camera"), 42ms);
  EXPECT_TRUE(service.sleep_for(0ns));
  std::exit(::testing::Test::HasFailure() ? 1 : 0);
}

auto verify_default_selection() -> void {
  auto& service = kitzoo::time::Time::instance();
  EXPECT_FALSE(service.init(nullptr));
  EXPECT_TRUE(service.is_valid());
  EXPECT_EQ(service.timeline_type(), kitzoo::time::TimelineType::System);
  const auto before = kitzoo::time::utc_timestamp().time_since_epoch();
  EXPECT_GE(service.timestamp(), before);
  EXPECT_FALSE(service.init(std::make_shared<kitzoo::time::FeederTimeline>()));
  std::exit(::testing::Test::HasFailure() ? 1 : 0);
}

auto verify_concurrent_default_selection() -> void {
  std::atomic<int> system{0};
  {
    std::vector<std::jthread> threads;
    for (int t = 0; t < 8; ++t)
      threads.emplace_back([&system] {
        for (int i = 0; i < 1000; ++i) {
          EXPECT_GT(Time::instance().timestamp(), 0ns);
          if (Time::instance().timeline_type() == TimelineType::System)
            ++system;
        }
      });
  }
  EXPECT_EQ(system.load(), 8000);
  EXPECT_FALSE(Time::instance().init(std::make_shared<FeederTimeline>()));
  std::exit(::testing::Test::HasFailure() ? 1 : 0);
}

TEST(TimelineDeathTest, ConcurrentFirstQueriesSelectOneTimeline) {
  EXPECT_EXIT(verify_concurrent_default_selection(), ::testing::ExitedWithCode(0), "");
}

TEST(TimelineDeathTest, ExplicitSelectionIsOneShotAndCallbackMayQueryService) {
  EXPECT_EXIT(verify_explicit_selection(), ::testing::ExitedWithCode(0), "");
}

TEST(TimelineDeathTest, FirstQuerySelectsSystemAndPreventsLaterInit) {
  EXPECT_EXIT(verify_default_selection(), ::testing::ExitedWithCode(0), "");
}

} // namespace
