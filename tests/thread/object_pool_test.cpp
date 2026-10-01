#include <kitzoo/thread/object_pool.hpp>

#include <gtest/gtest.h>
#include <string>

TEST(ObjectPoolTest, ConstructsAndDestroysObjects) {
    kitzoo::thread::ObjectPool<std::string> pool;
    auto* object = pool.construct("pooled");
    ASSERT_EQ(*object, "pooled");
    EXPECT_EQ(pool.allocated_count(), 1u);
    pool.destroy(object);
    EXPECT_EQ(pool.allocated_count(), 0u);
}

TEST(ObjectPoolTest, ReusesDestroyedSlots) {
    kitzoo::thread::ObjectPool<int> pool{1};
    auto* first = pool.construct(1);
    pool.destroy(first);
    auto* second = pool.construct(2);
    EXPECT_EQ(first, second);
    EXPECT_EQ(*second, 2);
    pool.destroy(second);
}

TEST(ObjectPoolTest, LeaseReturnsSlotAutomatically) {
    kitzoo::thread::ObjectPool<int> pool;
    {
        auto object = pool.acquire(42);
        EXPECT_EQ(*object, 42);
        EXPECT_EQ(pool.allocated_count(), 1u);
    }
    EXPECT_EQ(pool.allocated_count(), 0u);
}
