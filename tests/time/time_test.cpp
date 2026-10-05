// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: tests/time/time_test.cpp
// Description: Verifies basic clocks, calendar boundaries, steady timing and combined headers.
// -----------------------------------------------------------------------------

#include <kitzoo/time.hpp>
#include <kitzoo/time/time.hpp>

#include <chrono>
#include <gtest/gtest.h>
#include <stdexcept>
#include <thread>
#include <type_traits>

namespace {
using namespace kitzoo::time;
using namespace std::chrono_literals;
using Clock = std::chrono::steady_clock;

TEST(TimeStopwatchTest, ElapsedIsNonNegativeAndMonotonic) {
  Stopwatch watch;
  auto previous = watch.elapsed();
  EXPECT_GE(previous, Clock::duration::zero());
  for (int i = 0; i < 100; ++i) {
    const auto current = watch.elapsed();
    EXPECT_GE(current, previous);
    previous = current;
  }
}

TEST(TimeStopwatchTest, TypedElapsedMatchesDurationConversionBounds) {
  Stopwatch watch;
  const auto before = watch.elapsed();
  const auto microseconds = watch.elapsed_as<std::chrono::microseconds>();
  const auto after = watch.elapsed();
  EXPECT_GE(microseconds, std::chrono::duration_cast<std::chrono::microseconds>(before));
  EXPECT_LE(microseconds, std::chrono::duration_cast<std::chrono::microseconds>(after));
}

TEST(TimeStopwatchTest, ResetMeasuresFromNewOrigin) {
  Stopwatch watch;
  std::this_thread::sleep_for(1ms);
  EXPECT_GE(watch.elapsed(), 1ms);
  const auto before_reset = Clock::now();
  watch.reset();
  const auto elapsed = watch.elapsed();
  EXPECT_GE(elapsed, Clock::duration::zero());
  EXPECT_LE(elapsed, Clock::now() - before_reset);
}

TEST(TimeDeadlineTest, PreservesExplicitPastAndFutureTargets) {
  const auto now = Clock::now();
  const auto past = Deadline::at(now - 1h);
  const auto future = Deadline::at(now + 1h);
  EXPECT_EQ(past.time_point(), now - 1h);
  EXPECT_EQ(future.time_point(), now + 1h);
  EXPECT_TRUE(past.expired());
  EXPECT_LT(past.remaining(), Clock::duration::zero());
  EXPECT_FALSE(future.expired());
  EXPECT_GT(future.remaining(), Clock::duration::zero());
}

TEST(TimeDeadlineTest, RelativeZeroAndNegativeDurationsAreExpired) {
  EXPECT_TRUE(Deadline::after(0ns).expired());
  EXPECT_TRUE(Deadline::after(-1s).expired());
  const auto before = Clock::now();
  const auto deadline = Deadline::after(1h);
  EXPECT_GE(deadline.time_point(), before + 1h);
  EXPECT_LE(deadline.time_point(), Clock::now() + 1h);
}

TEST(TimeCalendarTimeTest, FormatsEveryPrecisionWithoutRounding) {
  const auto timestamp = from_date_time({2040, 2, 29, 12, 34, 56, 123456789});
  EXPECT_EQ(format_timestamp(timestamp, TimeZone::Utc, TimestampPrecision::Seconds), "2040-02-29 12:34:56");
  EXPECT_EQ(format_timestamp(timestamp, TimeZone::Utc, TimestampPrecision::Milliseconds), "2040-02-29 12:34:56.123");
  EXPECT_EQ(format_timestamp(timestamp, TimeZone::Utc, TimestampPrecision::Microseconds), "2040-02-29 12:34:56.123456");
  EXPECT_EQ(format_timestamp(timestamp, TimeZone::Utc, TimestampPrecision::Nanoseconds),
            "2040-02-29 12:34:56.123456789");
}

TEST(TimeCalendarTimeTest, LeapDateRoundTripPreservesAllFieldsBeyond2038) {
  const DateTime input{2040, 2, 29, 12, 34, 56, 123456789};
  const auto timestamp = from_date_time(input);
  const auto date = to_date_time(timestamp);
  EXPECT_EQ(date.year, input.year);
  EXPECT_EQ(date.month, input.month);
  EXPECT_EQ(date.day, input.day);
  EXPECT_EQ(date.hour, input.hour);
  EXPECT_EQ(date.minute, input.minute);
  EXPECT_EQ(date.second, input.second);
  EXPECT_EQ(date.nanosecond, input.nanosecond);
  EXPECT_EQ(date.weekday, 3u); // Wednesday
  EXPECT_EQ(date.day_of_year, 60u);
  EXPECT_EQ(from_date_time(date), timestamp);
}

TEST(TimeCalendarTimeTest, EpochAndNegativeFractionNormalizeCorrectly) {
  const auto epoch = to_date_time(TimeStamp{});
  EXPECT_EQ(epoch.year, 1970);
  EXPECT_EQ(epoch.weekday, 4u);
  EXPECT_EQ(epoch.day_of_year, 1u);
  EXPECT_EQ(format_timestamp(TimeStamp{}, TimeZone::Utc), "1970-01-01 00:00:00.000");
  EXPECT_EQ(format_timestamp(TimeStamp{-1ns}, TimeZone::Utc, TimestampPrecision::Nanoseconds),
            "1969-12-31 23:59:59.999999999");
  EXPECT_EQ(from_date_time({1969, 12, 31, 23, 59, 59, 999999999}), TimeStamp{-1ns});
}

TEST(TimeCalendarTimeTest, FullNanosecondRangeRoundTrips) {
  for (const auto value : {TimeDuration::min(), TimeDuration::max()}) {
    const TimeStamp timestamp{value};
    EXPECT_EQ(from_date_time(to_date_time(timestamp)), timestamp);
  }
  EXPECT_THROW(from_date_time({2500, 1, 1}), std::out_of_range);
}

TEST(TimeCalendarTimeTest, RejectsInvalidCalendarAndClockFields) {
  for (const DateTime date : {DateTime{2023, 2, 29}, DateTime{2024, 0, 1}, DateTime{2024, 13, 1}, DateTime{2024, 4, 31},
                              DateTime{2024, 1, 0}, DateTime{2024, 1, 1, 24}, DateTime{2024, 1, 1, 0, 60},
                              DateTime{2024, 1, 1, 0, 0, 60}, DateTime{2024, 1, 1, 0, 0, 0, 1000000000}})
    EXPECT_THROW(from_date_time(date), std::invalid_argument);
}

TEST(TimeCalendarTimeTest, DerivedFieldsDoNotAffectCalendarInput) {
  DateTime date{2024, 1, 1};
  const auto timestamp = from_date_time(date);
  date.weekday = 99;
  date.day_of_year = 999;
  EXPECT_EQ(from_date_time(date), timestamp);
  EXPECT_EQ(to_date_time(timestamp).weekday, 1u);
  EXPECT_EQ(to_date_time(timestamp).day_of_year, 1u);
}

TEST(TimeCalendarTimeTest, LocalRoundTripDoesNotAssumeMachineTimezone) {
  const auto timestamp = from_date_time({2024, 7, 15, 12, 34, 56, 987654321});
  const auto local = to_date_time(timestamp, TimeZone::Local);
  EXPECT_EQ(from_date_time(local, TimeZone::Local), timestamp);
  EXPECT_EQ(format_timestamp(timestamp).size(), 23u);
}

TEST(TimeClockTest, SystemAndSteadyReadingsUseTheirOwnDomains) {
  const auto before = std::chrono::system_clock::now();
  const auto timestamp = utc_timestamp();
  const auto after = std::chrono::system_clock::now();
  EXPECT_GE(timestamp, before);
  EXPECT_LE(timestamp, after);
  const auto steady = steady_timestamp();
  EXPECT_GE(steady_timestamp(), steady);
}

TEST(TimeClockTest, UnavailablePtpReturnsEmptyWithoutRequiringHardware) {
  EXPECT_FALSE(ptp_timestamp(""));
  EXPECT_FALSE(ptp_timestamp("/kitzoo_no_such_ptp_device"));
#if !defined(__linux__)
  EXPECT_FALSE(ptp_timestamp());
#endif
}

TEST(TimeIncludeTest, CombinedHeadersExposeExistingFacilities) {
  static_assert(std::is_same_v<kitzoo::time::TimeDuration, std::chrono::nanoseconds>);
  kitzoo::time::Stopwatch watch;
  EXPECT_GE(watch.elapsed(), std::chrono::steady_clock::duration::zero());
  EXPECT_TRUE(kitzoo::time::Deadline::after(std::chrono::nanoseconds::zero()).expired());
  EXPECT_EQ(kitzoo::time::to_date_time(kitzoo::time::TimeStamp{}).year, 1970);
  kitzoo::time::TimeWatcher watcher;
  auto scope = watcher.scope("combined");
  EXPECT_EQ(watcher.last_result("combined"), std::nullopt);
  EXPECT_GE(scope.finish(), kitzoo::time::TimeDuration::zero());
}

} // namespace
