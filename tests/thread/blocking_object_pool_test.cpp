#include <kitzoo/thread/blocking_object_pool.hpp>

#include <chrono>
#include <future>
#include <gtest/gtest.h>
#include <memory>
#include <thread>

using namespace std::chrono_literals;

TEST(BlockingObjectPoolTest, AcquireWaitsUntilObjectIsAdded) {
    kitzoo::thread::BlockingObjectPool<int> pool;
    auto acquired = std::async(std::launch::async, [&pool] { return pool.acquire(); });

    EXPECT_EQ(acquired.wait_for(10ms), std::future_status::timeout);
    pool.add(std::make_unique<int>(42));
    auto object = acquired.get();
    ASSERT_TRUE(object);
    EXPECT_EQ(*object, 42);
}

TEST(BlockingObjectPoolTest, ReturnedLeaseMakesObjectAvailableAgain) {
    kitzoo::thread::BlockingObjectPool<int> pool;
    pool.add(std::make_unique<int>(42));

    {
        auto object = pool.acquire();
        EXPECT_EQ(pool.available_count(), 0u);
    }
    EXPECT_EQ(pool.available_count(), 1u);
    EXPECT_EQ(*pool.acquire(), 42);
}

TEST(BlockingObjectPoolTest, SupportsTryAndTimedAcquire) {
    kitzoo::thread::BlockingObjectPool<int> pool;
    EXPECT_FALSE(pool.try_acquire());
    EXPECT_FALSE(pool.acquire_for(1ms));

    pool.add(std::make_unique<int>(7));
    auto object = pool.acquire_for(10ms);
    ASSERT_TRUE(object);
    EXPECT_EQ(**object, 7);
}

TEST(BlockingObjectPoolTest, SharedLeaseReturnsAfterLastOwner) {
    kitzoo::thread::BlockingObjectPool<int> pool;
    pool.add(std::make_unique<int>(9));

    auto first = pool.acquire_shared();
    auto second = first;
    first.reset();
    EXPECT_EQ(pool.available_count(), 0u);
    second.reset();
    EXPECT_EQ(pool.available_count(), 1u);
}

TEST(BlockingObjectPoolTest, CloseWakesWaitingAcquirer) {
    kitzoo::thread::BlockingObjectPool<int> pool;
    auto acquired = std::async(std::launch::async, [&pool] { return pool.acquire(); });
    EXPECT_EQ(acquired.wait_for(10ms), std::future_status::timeout);
    pool.close();

    auto object = acquired.get();
    EXPECT_FALSE(object);
}
