// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: tests/thread/synchronized_test.cpp
// Description: Verifies synchronized values and concurrent scoped access.
// -----------------------------------------------------------------------------

#include <kitzoo/thread/synchronized.hpp>

#include <gtest/gtest.h>
#include <shared_mutex>
#include <string>
#include <thread>
#include <vector>

TEST(SynchronizedTest, WithLockAndCopy) {
  kitzoo::thread::Synchronized<std::string> value{std::string{"hello"}};
  value.with_lock([](auto& text) -> void { text += " world"; });
  EXPECT_EQ(value.copy(), "hello world");
}

TEST(SynchronizedTest, ConcurrentIncrements) {
  kitzoo::thread::Synchronized<int> counter{0};
  std::vector<std::jthread> threads;
  for (int i = 0; i < 8; ++i) {
    threads.emplace_back([&counter] {
      for (int j = 0; j < 1000; ++j)
        counter.with_lock([](int& value) -> void { ++value; });
    });
  }
  threads.clear();
  EXPECT_EQ(counter.copy(), 8000);
}

TEST(SynchronizedTest, SharedMutexRead) {
  kitzoo::thread::Synchronized<int, std::shared_mutex> value{42};
  EXPECT_EQ(value.read([](const int& number) -> int { return number * 2; }), 84);
}
