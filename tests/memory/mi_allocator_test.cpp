// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: tests/memory/mi_allocator_test.cpp
// Description: Verifies direct mimalloc allocation through standard containers.
// -----------------------------------------------------------------------------

#include <kitzoo/memory/mi_allocator.hpp>

#include <gtest/gtest.h>
#include <limits>
#include <type_traits>
#include <vector>

TEST(MiAllocatorTest, WorksWithStandardContainers) {
    std::vector<int, kitzoo::memory::MiAllocator<int>> values;
    values.push_back(1);
    values.push_back(2);
    ASSERT_EQ(values.size(), 2u);
    EXPECT_EQ(values[0], 1);
    EXPECT_EQ(values[1], 2);
}

TEST(MiAllocatorTest, StatelessAlignedZeroAndOverflowAllocation) {
    using kitzoo::memory::MiAllocator;

    struct alignas(256) Aligned {
        int value = 42;
    };

    static_assert(std::is_empty_v<MiAllocator<int>>);
    static_assert(std::allocator_traits<MiAllocator<int>>::is_always_equal::value);
    MiAllocator<Aligned> allocator;
    auto* address = allocator.allocate(2);
    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(address) % 256, 0U);
    allocator.deallocate(address, 2);
    address = allocator.allocate(0);
    ASSERT_NE(address, nullptr);
    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(address) % 256, 0U);
    allocator.deallocate(address, 0);
    EXPECT_THROW(static_cast<void>(allocator.allocate(std::numeric_limits<std::size_t>::max())),
                 std::bad_array_new_length);
}

TEST(MiAllocatorTest, MoveTransfersStorageWithoutAllocatorPropagation) {
    using Values = std::vector<int, kitzoo::memory::MiAllocator<int>>;
    static_assert(!std::allocator_traits<
                  Values::allocator_type>::propagate_on_container_move_assignment::value);
    Values source{1, 2, 3};
    auto* original = source.data();
    Values destination{9};
    destination = std::move(source);
    EXPECT_EQ(destination.data(), original);

    ASSERT_EQ(destination.size(), 3U);
    EXPECT_EQ(destination[2], 3);
}
