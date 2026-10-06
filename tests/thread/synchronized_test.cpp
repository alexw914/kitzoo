// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: tests/thread/synchronized_test.cpp
// Description: Verifies synchronized values and concurrent scoped access.
// -----------------------------------------------------------------------------

#include <kitzoo/thread/spinlock.hpp>
#include <kitzoo/thread/synchronized.hpp>

#include <chrono>
#include <cstddef>
#include <future>
#include <gtest/gtest.h>
#include <mutex>
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

TEST(SynchronizedTest, SpinLockGuardsConcurrentIncrements) {
  kitzoo::thread::Synchronized<int, kitzoo::thread::SpinLock> counter{0};
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

TEST(SynchronizedTest, RWSpinLockSupportsSharedReadAndExclusiveWrite) {
  kitzoo::thread::Synchronized<std::string, kitzoo::thread::RWSpinLock> value{std::string{"a"}};
  value.with_lock([](std::string& text) -> void { text += "b"; });
  EXPECT_EQ(value.read([](const std::string& text) -> std::size_t { return text.size(); }), 2u);
  EXPECT_EQ(value.copy(), "ab");
}

TEST(SynchronizedTest, ReadIsOnlyAvailableForSharedLockableMutexes) {
  constexpr auto can_read = []<typename Mutex>() -> bool {
    return requires(const kitzoo::thread::Synchronized<int, Mutex>& value) {
      value.read([](const int& number) -> int { return number; });
    };
  };
  EXPECT_TRUE(can_read.template operator()<std::shared_mutex>());
  EXPECT_TRUE(can_read.template operator()<kitzoo::thread::RWSpinLock>());
  EXPECT_FALSE(can_read.template operator()<std::mutex>());
  EXPECT_FALSE(can_read.template operator()<kitzoo::thread::SpinLock>());
}

// copy() must not wait for a reader that is still holding its shared lock.
TEST(SynchronizedTest, CopyRunsAlongsideSharedReaders) {
  kitzoo::thread::Synchronized<int, std::shared_mutex> value{7};
  const bool copied = value.read([&value](const int&) -> bool {
    auto copy = std::async(std::launch::async, [&value] { return value.copy(); });
    return copy.wait_for(std::chrono::seconds{5}) == std::future_status::ready && copy.get() == 7;
  });
  EXPECT_TRUE(copied);
}
