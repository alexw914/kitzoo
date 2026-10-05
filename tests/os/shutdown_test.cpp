// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: tests/os/shutdown_test.cpp
// Description: Verifies shutdown requests, waiting, and signal handling; each
//              scenario runs in its own process because the state is global.
// -----------------------------------------------------------------------------

#include <kitzoo/os/shutdown.hpp>

#include <chrono>
#include <csignal>
#include <cstdlib>
#include <gtest/gtest.h>
#include <thread>

using namespace std::chrono_literals;

namespace {

auto check(bool condition) -> void {
  if (!condition)
    std::exit(1);
}

} // namespace

TEST(ShutdownTest, WaitTimesOutUntilRequested) {
  EXPECT_EXIT(
      {
        check(!kitzoo::os::shutdown_requested());
        const auto start = std::chrono::steady_clock::now();
        check(!kitzoo::os::wait_for_shutdown(20ms));
        check(std::chrono::steady_clock::now() - start >= 20ms);
        kitzoo::os::request_shutdown();
        check(kitzoo::os::shutdown_requested());
        check(kitzoo::os::wait_for_shutdown(20ms));
        std::exit(0);
      },
      testing::ExitedWithCode(0), "");
}

TEST(ShutdownTest, RequestWakesBlockedWaiters) {
  EXPECT_EXIT(
      {
        std::jthread first{[] { check(kitzoo::os::wait_for_shutdown()); }};
        std::jthread second{[] { check(kitzoo::os::wait_for_shutdown(10s)); }};
        std::this_thread::sleep_for(20ms);
        kitzoo::os::request_shutdown();
        first.join();
        second.join();
        std::exit(0);
      },
      testing::ExitedWithCode(0), "");
}

#if !defined(_WIN32)
TEST(ShutdownTest, FirstSignalRequestsShutdown) {
  EXPECT_EXIT(
      {
        kitzoo::os::install_shutdown_handler();
        kitzoo::os::install_shutdown_handler();
        std::raise(SIGTERM);
        check(kitzoo::os::wait_for_shutdown(1s));
        std::exit(0);
      },
      testing::ExitedWithCode(0), "");
}

TEST(ShutdownTest, SecondSignalUsesDefaultAction) {
  EXPECT_EXIT(
      {
        kitzoo::os::install_shutdown_handler();
        std::raise(SIGINT);
        std::raise(SIGINT);
        std::exit(0);
      },
      testing::KilledBySignal(SIGINT), "");
}
#endif
