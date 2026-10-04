// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: tests/memory/resource_test.cpp
// Description: Verifies budgets, resource propagation, alignment and allocation rollback.
// -----------------------------------------------------------------------------

#include <kitzoo/memory/memory.hpp>
#include <kitzoo/memory/resource.hpp>

#include <atomic>
#include <gtest/gtest.h>
#include <thread>

namespace {

class FailingResource final : public std::pmr::memory_resource {
    auto do_allocate(std::size_t, std::size_t) -> void* override { throw std::bad_alloc{}; }

    auto do_deallocate(void*, std::size_t, std::size_t) -> void override {}

    auto do_is_equal(std::pmr::memory_resource const& other) const noexcept -> bool override {
        return this == &other;
    }
};

struct alignas(256) AlignedValue {
    int value = 42;
};

}  // namespace

TEST(MemoryResourceTest, LimitsAlignmentStatisticsAndRollback) {
    using namespace kitzoo::memory;
    LimitedResource budget{1024};
    EXPECT_EQ(budget.capacity(), 1024U);
    EXPECT_THROW(LimitedResource(1, nullptr), std::invalid_argument);
    auto* address = budget.allocate(512, 256);
    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(address) % 256, 0U);
    EXPECT_TRUE(budget.owns(address));
    EXPECT_EQ(budget.stats().used_bytes, 512U);
    EXPECT_EQ(budget.stats().allocation_count, 1U);
    EXPECT_THROW(static_cast<void>(budget.allocate(513)), std::bad_alloc);
    EXPECT_FALSE(budget.release(address, 511));
    EXPECT_EQ(budget.stats().used_bytes, 512U);
    budget.deallocate(address, 512, 128);  // Reject a mismatched alignment.
    EXPECT_TRUE(budget.owns(address));
    EXPECT_TRUE(budget.release(address, 512));
    EXPECT_FALSE(budget.owns(address));
    EXPECT_EQ(budget.stats().used_bytes, 0U);
    EXPECT_EQ(budget.stats().peak_bytes, 512U);
    EXPECT_EQ(budget.stats().allocation_count, 0U);
    EXPECT_EQ(budget.try_allocate(1, 3), nullptr);
    FailingResource failing;
    LimitedResource failure_budget{1024, &failing};
    EXPECT_THROW(static_cast<void>(failure_budget.allocate(4)), std::bad_alloc);
    EXPECT_EQ(failure_budget.stats().used_bytes, 0U);
    EXPECT_EQ(failure_budget.stats().allocation_count, 0U);
    LimitedResource outer{256, &budget};
    address = outer.allocate(256);
    EXPECT_EQ(budget.stats().used_bytes, 256U);
    outer.deallocate(address, 256);
    EXPECT_EQ(budget.stats().used_bytes, 0U);
}

TEST(MemoryResourceTest, ContainerResourcePropagationAndObjectHelpers) {
    using namespace kitzoo::memory;
    LimitedResource first{65536};
    LimitedResource second{65536};
    EXPECT_NE(std::pmr::polymorphic_allocator<int>{&first},
              std::pmr::polymorphic_allocator<int>{&second});
    EXPECT_EQ(std::pmr::polymorphic_allocator<int>{&first},
              std::pmr::polymorphic_allocator<char>{&first});
    {
        std::pmr::vector<std::pmr::string> nested{
            std::pmr::polymorphic_allocator<std::pmr::string>{&first}};
        nested.emplace_back(100, 'x');
        EXPECT_EQ(nested[0].get_allocator().resource(), &first);
        EXPECT_TRUE(first.owns(nested[0].data()));
        // Standard PMR copy construction needs an explicit resource to retain it.
        auto copy = decltype(nested){nested, &first};
        EXPECT_EQ(copy.get_allocator().resource(), &first);
        EXPECT_EQ(copy[0].get_allocator().resource(), &first);
        std::pmr::vector<std::pmr::string> destination{
            std::pmr::polymorphic_allocator<std::pmr::string>{&second}};
        destination = std::move(copy);
        EXPECT_EQ(destination.get_allocator().resource(), &second);
        EXPECT_EQ(destination[0].get_allocator().resource(), &second);
        std::pmr::map<int, std::pmr::string> names{&first};
        names.try_emplace(1, 100, 'y');
        EXPECT_EQ(names.at(1).get_allocator().resource(), &first);
        Memory objects{&first};
        auto object = objects.alloc_object<std::pmr::string>(100, 'z');
        EXPECT_EQ(object->get_allocator().resource(), &first);
        auto strings = objects.alloc_basic_array<std::pmr::string>(2);
        strings[0].assign(100, 'a');
        EXPECT_EQ(strings[0].get_allocator().resource(), &first);
        auto aligned = objects.alloc_object<AlignedValue>();
        EXPECT_EQ(reinterpret_cast<std::uintptr_t>(aligned.get()) % 256, 0U);
        auto allocator = std::pmr::polymorphic_allocator<std::pmr::string>{&second};
        auto* raw = allocator.new_object<std::pmr::string>(100, 'b');
        EXPECT_EQ(raw->get_allocator().resource(), &second);
        allocator.delete_object(raw);
    }
    EXPECT_EQ(first.stats().used_bytes, 0U);
    EXPECT_EQ(second.stats().used_bytes, 0U);
}

TEST(MemoryResourceTest, WeakPointerRetainsControlBlockUntilReset) {
    using namespace kitzoo::memory;
    LimitedResource budget{4096};
    WeakPtr<int> weak;
    {
        auto pointer = kitzoo::memory::make_shared_with_resource<int>(&budget, 42);
        weak = pointer;
        auto locked = weak.lock();
        ASSERT_NE(locked, nullptr);
        EXPECT_EQ(locked.get(), pointer.get());
        EXPECT_EQ(*locked, 42);
    }
    EXPECT_TRUE(weak.expired());
    EXPECT_FALSE(weak.lock());
    EXPECT_GT(budget.stats().used_bytes, 0U);
    weak.reset();
    EXPECT_EQ(budget.stats().used_bytes, 0U);
    EXPECT_EQ(budget.stats().allocation_count, 0U);
}

TEST(MemoryResourceTest, ArrayControlBlockFailureReclaimsStorage) {
    using namespace kitzoo::memory;
    LimitedResource tiny{sizeof(int)};
    Memory objects{&tiny};
    EXPECT_THROW(objects.alloc_basic_array<int>(1), std::bad_alloc);
    EXPECT_EQ(tiny.stats().used_bytes, 0U);
}

TEST(MemoryResourceTest, ConcurrentBudgetNeverExceedsCapacity) {
    using namespace kitzoo::memory;
    LimitedResource budget{256};
    std::atomic<bool> failed = false;
    std::vector<std::thread> workers;
    for (int i = 0; i < 8; ++i) {
        workers.emplace_back([&]() -> void {
            for (int iteration = 0; iteration < 1000; ++iteration) {
                auto* address = budget.try_allocate(64, 64);
                if (address)
                    budget.deallocate(address, 64, 64);
                if (budget.stats().used_bytes > budget.capacity())
                    failed = true;
            }
        });
    }
    for (auto& worker : workers)
        worker.join();
    EXPECT_FALSE(failed);
    EXPECT_LE(budget.stats().peak_bytes, budget.capacity());
    EXPECT_EQ(budget.stats().used_bytes, 0U);
    EXPECT_EQ(budget.stats().allocation_count, 0U);
}
