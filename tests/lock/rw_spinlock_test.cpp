#include <kitzoo/lock/spinlock.hpp>

#include <gtest/gtest.h>
#include <shared_mutex>

TEST(RWSpinLockTest, SupportsSharedAndExclusiveLocking) {
    kitzoo::lock::RWSpinLock lock;
    {
        std::shared_lock first{lock};
        std::shared_lock second{lock};
        EXPECT_FALSE(lock.try_lock());
    }
    std::unique_lock writer{lock};
    EXPECT_FALSE(lock.try_lock_shared());
}
