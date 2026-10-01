#include <kitzoo/memory.hpp>

#include <gtest/gtest.h>
#include <vector>

TEST(MimallocAllocatorTest, WorksWithStandardContainers) {
    std::vector<int, kitzoo::memory::MimallocAllocator<int>> values;
    values.push_back(1);
    values.push_back(2);
    ASSERT_EQ(values.size(), 2u);
    EXPECT_EQ(values[0], 1);
    EXPECT_EQ(values[1], 2);
}
