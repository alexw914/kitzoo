// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: tests/memory/memory_test.cpp
// Description: Verifies object and array lifetime, alignment and construction rollback.
// -----------------------------------------------------------------------------

#include <kitzoo/memory/memory.hpp>
#include <kitzoo/memory/resource.hpp>

#include <cstdint>
#include <gtest/gtest.h>
#include <limits>
#include <stdexcept>

namespace {

constexpr std::size_t kBudgetBytes = 65536;

struct alignas(64) Tracked {
  static inline int live = 0;
  int value = 42;

  Tracked() { ++live; }

  ~Tracked() { --live; }
};

struct Throwing {
  static inline int live = 0;
  static inline int attempts = 0;

  Throwing() {
    if (++attempts == 3)
      throw std::runtime_error("construction failed");
    ++live;
  }

  ~Throwing() { --live; }
};

} // namespace

TEST(MemoryTest, ObjectAndArrayLifetimeAlignmentAndExceptions) {
  using namespace kitzoo::memory;
  LimitedResource budget{kBudgetBytes};
  Memory memory{&budget};
  {
    auto shared = memory.alloc_object<Tracked>();
    auto array = memory.alloc_basic_array<Tracked>(4);
    EXPECT_EQ(Tracked::live, 5);
    EXPECT_EQ(array[3].value, 42);
    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(array.get()) % alignof(Tracked), 0U);
  }
  EXPECT_EQ(Tracked::live, 0);
  auto zeros = memory.alloc_basic_array<int>(4);
  EXPECT_EQ(zeros[3], 0);
  EXPECT_FALSE(memory.alloc_basic_array<int>(0));
  EXPECT_THROW(memory.alloc_basic_array<int>(std::numeric_limits<std::size_t>::max()), std::bad_array_new_length);
  Throwing::attempts = 0;
  EXPECT_THROW(memory.alloc_basic_array<Throwing>(4), std::runtime_error);
  EXPECT_EQ(Throwing::live, 0);
  zeros.reset();
  EXPECT_EQ(budget.stats().used_bytes, 0U); // Constructor failures reclaimed storage.
}

TEST(MemoryTest, DefaultResourceSupportsAlignedObjectsAndArrays) {
  kitzoo::memory::Memory objects;
  {
    auto object = objects.alloc_object<Tracked>();
    auto array = objects.alloc_basic_array<Tracked>(3);
    EXPECT_EQ(Tracked::live, 4);
    EXPECT_EQ(array[2].value, 42);
    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(object.get()) % alignof(Tracked), 0U);
    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(array.get()) % alignof(Tracked), 0U);
  }
  EXPECT_EQ(Tracked::live, 0);
}
