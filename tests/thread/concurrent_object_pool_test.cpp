// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: tests/thread/concurrent_object_pool_test.cpp
// Description: Verifies concurrent pool ownership, cross-thread return and shared leases.
// -----------------------------------------------------------------------------

#include <kitzoo/thread/object_pool.hpp>

#include <atomic>
#include <chrono>
#include <future>
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

using namespace std::chrono_literals;
using kitzoo::thread::ConcurrentObjectPool;

TEST(ConcurrentObjectPoolTest, AcquireWaitsUntilObjectIsAdded) {
  ConcurrentObjectPool<int> pool;
  auto acquired = std::async(std::launch::async, [&pool]() -> decltype(pool.acquire()) { return pool.acquire(); });

  auto status = acquired.wait_for(10ms);
  pool.add(kitzoo::memory::make_unique<int>(42));
  EXPECT_EQ(status, std::future_status::timeout);
  ASSERT_EQ(acquired.wait_for(1s), std::future_status::ready);
  auto object = acquired.get();
  ASSERT_TRUE(object);
  EXPECT_EQ(*object, 42);
}

TEST(ConcurrentObjectPoolTest, LeaseReturnsObjectToPool) {
  ConcurrentObjectPool<std::string> pool;
  pool.add(kitzoo::memory::make_unique<std::string>("reused"));

  {
    auto object = pool.acquire();
    EXPECT_EQ(*object, "reused");
    EXPECT_EQ(pool.available_approx(), 0u);
  }

  EXPECT_EQ(pool.available_approx(), 1u);
}

TEST(ConcurrentObjectPoolTest, SupportsNonblockingAndTimedAcquire) {
  ConcurrentObjectPool<int> pool;
  EXPECT_FALSE(pool.try_acquire());
  EXPECT_FALSE(pool.acquire_for(1ms));

  pool.add(kitzoo::memory::make_unique<int>(42));
  auto object = pool.acquire_for(10ms);
  ASSERT_TRUE(object);
  EXPECT_EQ(**object, 42);
}

TEST(ConcurrentObjectPoolTest, LeaseCanReturnFromAnotherThread) {
  ConcurrentObjectPool<int> pool;
  pool.add(kitzoo::memory::make_unique<int>(7));
  auto object = pool.acquire();

  std::thread worker{[lease = std::move(object)]() mutable { lease.reset(); }};
  worker.join();

  EXPECT_EQ(pool.available_approx(), 1u);
  EXPECT_EQ(*pool.acquire(), 7);
}

TEST(ConcurrentObjectPoolTest, SupportsConcurrentAcquireAndReturn) {
  struct Counter {
    std::atomic<int> uses{0};
  };

  ConcurrentObjectPool<Counter> pool;
  constexpr int object_count = 4;
  constexpr int thread_count = 8;
  constexpr int iterations = 200;
  for (int i = 0; i < object_count; ++i)
    pool.add(kitzoo::memory::make_unique<Counter>());

  std::vector<std::thread> workers;
  for (int i = 0; i < thread_count; ++i) {
    workers.emplace_back([&pool] {
      for (int j = 0; j < iterations; ++j) {
        auto object = pool.acquire();
        object->uses.fetch_add(1, std::memory_order_relaxed);
      }
    });
  }
  for (auto& worker : workers)
    worker.join();

  int total = 0;
  std::vector<decltype(pool.acquire())> objects;
  for (int i = 0; i < object_count; ++i) {
    objects.push_back(pool.acquire());
    total += objects.back()->uses.load(std::memory_order_relaxed);
  }
  EXPECT_EQ(total, thread_count * iterations);
  EXPECT_EQ(pool.available_approx(), 0u);
  objects.clear();
  EXPECT_EQ(pool.available_approx(), static_cast<std::size_t>(object_count));
}

TEST(ConcurrentObjectPoolTest, SharedLeaseReturnsAfterLastOwner) {
  ConcurrentObjectPool<int> pool;
  pool.add(kitzoo::memory::make_unique<int>(9));
  static_assert(std::is_same_v<decltype(pool.acquire_shared()), kitzoo::memory::SharedPtr<int>>);

  auto first = pool.acquire_shared();
  auto second = first;
  kitzoo::memory::WeakPtr<int> weak = first;
  first.reset();
  EXPECT_EQ(pool.available_approx(), 0u);

  second.reset();
  EXPECT_EQ(pool.available_approx(), 1u);
  EXPECT_TRUE(weak.expired());
  EXPECT_FALSE(weak.lock());
  EXPECT_EQ(*pool.acquire(), 9);
  weak.reset();
}

TEST(ConcurrentObjectPoolTest, RejectsNullObjects) {
  ConcurrentObjectPool<int> pool;
  EXPECT_THROW(pool.add(nullptr), std::invalid_argument);
}
