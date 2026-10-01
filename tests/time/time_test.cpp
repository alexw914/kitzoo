// ---------------------------------------------------------------------------
// kitzoo/time tests
// ---------------------------------------------------------------------------

#include <kitzoo/time/time.hpp>

#include <chrono>
#include <gtest/gtest.h>
#include <regex>
#include <thread>

using namespace kitzoo;
using namespace kitzoo::time;
using namespace std::chrono_literals;

TEST(StopwatchTest, ElapsedIsNonNegative) {
    Stopwatch sw;
    EXPECT_GE(sw.elapsed().count(), 0);
}

TEST(StopwatchTest, MeasuresSleep) {
    Stopwatch sw;
    std::this_thread::sleep_for(20ms);
    EXPECT_GE(sw.elapsed_as(), std::chrono::milliseconds{15});  // slack for scheduling
}

TEST(StopwatchTest, ResetRestarts) {
    Stopwatch sw;
    std::this_thread::sleep_for(10ms);
    auto const before = sw.elapsed();
    sw.reset();
    auto const after = sw.elapsed();
    EXPECT_LT(after, before);
}

TEST(StopwatchTest, TypedElapsed) {
    Stopwatch sw;
    std::this_thread::sleep_for(1ms);
    EXPECT_GE(sw.elapsed_as<std::chrono::microseconds>().count(), 500);
}

TEST(DeadlineTest, NotExpiredInitially) {
    auto const dl = Deadline::after(1h);
    EXPECT_FALSE(dl.expired());
    EXPECT_GT(dl.remaining().count(), 0);
}

TEST(DeadlineTest, Expires) {
    auto const dl = Deadline::after(5ms);
    std::this_thread::sleep_for(20ms);
    EXPECT_TRUE(dl.expired());
    EXPECT_LT(dl.remaining().count(), 0);
}

TEST(DeadlineTest, AlreadyPast) {
    auto const dl = Deadline::at(std::chrono::steady_clock::now() - 1s);
    EXPECT_TRUE(dl.expired());
}

TEST(TimestampTest, FormatShape) {
    auto const s = format_timestamp();
    // "YYYY-MM-DD HH:MM:SS.mmm"
    static const std::regex pattern{R"(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}\.\d{3})"};
    EXPECT_TRUE(std::regex_match(s, pattern)) << "got: " << s;
}

TEST(TimestampTest, KnownEpoch) {
    std::chrono::system_clock::time_point const epoch{};
    auto const s = format_timestamp(epoch);
    // Local time near 1970-01-01 (offset varies by timezone)
    EXPECT_TRUE(s.starts_with("1969-12-31") || s.starts_with("1970-01-01"));
    EXPECT_TRUE(s.ends_with(".000"));
}
