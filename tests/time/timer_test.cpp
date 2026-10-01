#include <kitzoo/time/timer.hpp>

#include <atomic>
#include <chrono>
#include <gtest/gtest.h>
#include <thread>

using namespace std::chrono_literals;

TEST(TimerTest, FiresAndStopsPromptly) {
    std::atomic<int> calls{0};
    kitzoo::time::Timer timer{5ms};
    timer.start([&] { calls.fetch_add(1, std::memory_order_relaxed); });
    std::this_thread::sleep_for(30ms);
    timer.stop();

    auto const stopped_at = calls.load(std::memory_order_relaxed);
    EXPECT_GT(stopped_at, 0);
    EXPECT_FALSE(timer.running());
    std::this_thread::sleep_for(10ms);
    EXPECT_EQ(calls.load(std::memory_order_relaxed), stopped_at);
}

TEST(TimerTest, StopInterruptsLongWait) {
    kitzoo::time::Timer timer{1h};
    timer.start([] {});
    auto const start = std::chrono::steady_clock::now();
    timer.stop();
    EXPECT_LT(std::chrono::steady_clock::now() - start, 1s);
}

TEST(TimerTest, RejectsNonPositiveInterval) {
    EXPECT_THROW((kitzoo::time::Timer{0ms}), std::invalid_argument);
}
