// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: tests/thread/object_pool_test.cpp
// Description: Verifies object pool reuse, aligned storage and lease ownership.
// -----------------------------------------------------------------------------

#include <kitzoo/thread/object_pool.hpp>

#include <cstdint>
#include <gtest/gtest.h>
#include <stdexcept>
#include <string>
#include <type_traits>

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

TEST(ObjectPoolTest, GrowthPreservesAlignedLiveObjects) {
    struct alignas(256) Aligned {
        int value;
    };

    kitzoo::thread::ObjectPool<Aligned> pool{1};
    auto first = pool.acquire(42);
    auto* original = first.get();
    auto second = pool.acquire(7);
    EXPECT_EQ(first.get(), original);
    EXPECT_EQ(first->value, 42);
    EXPECT_EQ(second->value, 7);
    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(first.get()) % alignof(Aligned), 0U);
    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(second.get()) % alignof(Aligned), 0U);
    EXPECT_EQ(pool.allocated_count(), 2U);
}

TEST(ObjectPoolTest, SharedLeaseReturnsSlotAndExpiresWeakReference) {
    using Pool = kitzoo::thread::ObjectPool<int>;
    Pool pool{1};
    static_assert(std::is_same_v<decltype(pool.acquire_shared()), kitzoo::memory::SharedPtr<int>>);
    kitzoo::memory::WeakPtr<int> weak;
    int* original;
    {
        auto object = pool.acquire_shared(42);
        original = object.get();
        weak = object;
        auto locked = weak.lock();
        object.reset();
        ASSERT_NE(locked, nullptr);
        EXPECT_EQ(*locked, 42);
        EXPECT_EQ(pool.allocated_count(), 1U);
    }
    EXPECT_TRUE(weak.expired());
    EXPECT_EQ(pool.allocated_count(), 0U);
    auto reused = pool.acquire(7);
    EXPECT_EQ(reused.get(), original);
}

TEST(ObjectPoolTest, ConstructorFailureLeavesSlotReusable) {
    struct Value {
        explicit Value(bool fail) {
            if (fail)
                throw std::runtime_error("construction failed");
        }
    };

    kitzoo::thread::ObjectPool<Value> pool{1};
    EXPECT_THROW(static_cast<void>(pool.acquire(true)), std::runtime_error);
    EXPECT_EQ(pool.allocated_count(), 0U);
    auto object = pool.acquire(false);
    EXPECT_EQ(pool.allocated_count(), 1U);
}
