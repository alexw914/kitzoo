// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: tests/thread/spinlock_test.cpp
// Description: Verifies exclusive and reader-writer spin locks.
// -----------------------------------------------------------------------------

#include <kitzoo/thread/spinlock.hpp>

#include <gtest/gtest.h>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <vector>

TEST(SpinLockTest, ProtectsSharedState) {
  kitzoo::thread::SpinLock lock;
  int value = 0;
  std::vector<std::thread> threads;
  for (int i = 0; i < 4; ++i) {
    threads.emplace_back([&] {
      for (int j = 0; j < 10000; ++j) {
        std::lock_guard guard{lock};
        ++value;
      }
    });
  }
  for (auto& thread : threads)
    thread.join();
  EXPECT_EQ(value, 40000);
}

TEST(SpinLockRwTest, SupportsSharedAndExclusiveLocking) {
  kitzoo::thread::RWSpinLock lock;
  {
    std::shared_lock first{lock};
    std::shared_lock second{lock};
    EXPECT_FALSE(lock.try_lock());
  }
  std::unique_lock writer{lock};
  EXPECT_FALSE(lock.try_lock_shared());
}
