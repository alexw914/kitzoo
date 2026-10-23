// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: tests/memory/memory_test.cpp
// Description: Verifies container aliases, ownership, alignment, construction rollback
//              and the mimalloc process allocator.
// -----------------------------------------------------------------------------

#include <kitzoo/memory/memory.hpp>
#include <kitzoo/memory/resource.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <gtest/gtest.h>
#include <limits>
#include <map>
#include <memory>
#include <mimalloc.h>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace {

constexpr std::size_t kBudgetBytes = 65536;

struct alignas(64) Tracked {
  static inline int live_ = 0;
  int value = 42;

  Tracked() { ++live_; }

  ~Tracked() { --live_; }
};

struct Throwing {
  static inline int live_ = 0;
  static inline int attempts_ = 0;

  Throwing() {
    if (++attempts_ == 3)
      throw std::runtime_error("construction failed");
    ++live_;
  }

  ~Throwing() { --live_; }
};

} // namespace

TEST(MemoryTest, ObjectAndArrayLifetimeAlignmentAndExceptions) {
  using namespace kitzoo::memory;
  LimitedResource budget{kBudgetBytes};
  Memory memory{&budget};
  {
    auto shared = memory.alloc_object<Tracked>();
    auto array = memory.alloc_basic_array<Tracked>(4);
    EXPECT_EQ(Tracked::live_, 5);
    EXPECT_EQ(array[3].value, 42);
    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(array.get()) % alignof(Tracked), 0U);
  }
  EXPECT_EQ(Tracked::live_, 0);
  auto zeros = memory.alloc_basic_array<int>(4);
  EXPECT_EQ(zeros[3], 0);
  EXPECT_FALSE(memory.alloc_basic_array<int>(0));
  EXPECT_THROW(memory.alloc_basic_array<int>(std::numeric_limits<std::size_t>::max()), std::bad_array_new_length);
  Throwing::attempts_ = 0;
  EXPECT_THROW(memory.alloc_basic_array<Throwing>(4), std::runtime_error);
  EXPECT_EQ(Throwing::live_, 0);
  zeros.reset();
  EXPECT_EQ(budget.stats().used_bytes, 0U); // Constructor failures reclaimed storage.
}

TEST(MemoryTest, DefaultResourceSupportsAlignedObjectsAndArrays) {
  kitzoo::memory::Memory objects;
  {
    auto object = objects.alloc_object<Tracked>();
    auto array = objects.alloc_basic_array<Tracked>(3);
    EXPECT_EQ(Tracked::live_, 4);
    EXPECT_EQ(array[2].value, 42);
    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(object.get()) % alignof(Tracked), 0U);
    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(array.get()) % alignof(Tracked), 0U);
  }
  EXPECT_EQ(Tracked::live_, 0);
}

// Public container aliases and smart pointer factories.
namespace {

struct alignas(256) FactoryTracked {
  static inline int live_ = 0;
  int value;

  explicit FactoryTracked(int input) : value(input) { ++live_; }

  ~FactoryTracked() { --live_; }
};

struct FactoryThrowing {
  explicit FactoryThrowing(int) { throw std::runtime_error("constructor failed"); }
};

} // namespace

TEST(MemoryUniquePtrTest, ConstructionMoveResetAndExplicitRelease) {
  namespace memory = kitzoo::memory;
  static_assert(!std::is_copy_constructible_v<memory::UniquePtr<FactoryTracked>>);
  static_assert(std::is_nothrow_move_constructible_v<memory::UniquePtr<FactoryTracked>>);
  static_assert(std::is_same_v<decltype(memory::make_unique<FactoryTracked>(42)), memory::UniquePtr<FactoryTracked>>);
  {
    auto object = memory::make_unique<FactoryTracked>(42);
    EXPECT_EQ(object->value, 42);
    EXPECT_EQ(FactoryTracked::live_, 1);
    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(object.get()) % 256, 0U);
    auto moved = std::move(object);
    EXPECT_FALSE(object);
    auto* raw = moved.release();
    EXPECT_FALSE(moved);
    memory::UniquePtr<FactoryTracked> adopted{raw};
    auto replacement = memory::make_unique<FactoryTracked>(7);
    adopted.reset(replacement.release());
    EXPECT_FALSE(replacement);
    EXPECT_EQ(FactoryTracked::live_, 1);
    EXPECT_EQ(adopted->value, 7);
    adopted.reset();
    EXPECT_EQ(FactoryTracked::live_, 0);
  }
  EXPECT_EQ(FactoryTracked::live_, 0);
  EXPECT_EQ(*memory::make_unique<int>(), 0);
  auto text = memory::make_unique<memory::String>(100, 'x');
  EXPECT_EQ(text->size(), 100U);
  std::unique_ptr<int> standard = std::make_unique<int>(9);
  EXPECT_EQ(*standard, 9);
}

TEST(MemoryUniquePtrTest, ConstructorExceptionPropagates) {
  EXPECT_THROW(static_cast<void>(kitzoo::memory::make_unique<FactoryThrowing>(1)), std::runtime_error);
}

TEST(MemoryTypesTest, ContainerAliases) {
  using namespace kitzoo::memory;
  static_assert(std::is_same_v<String, std::string>);
  static_assert(std::is_same_v<Array<int, 3>, std::array<int, 3>>);
  static_assert(std::is_same_v<Vector<int>, std::vector<int>>);
  static_assert(std::is_same_v<Map<int, int>, std::map<int, int>>);
  static_assert(std::is_same_v<UniquePtr<int>, std::unique_ptr<int>>);
  Vector<int> values{1, 2, 3};
  EXPECT_EQ(values[2], 3);
  Map<int, String> names;
  names.emplace(1, String(80, 'x'));
  EXPECT_EQ(names.at(1).size(), 80U);
  UnorderedMap<int, int> lookup{{1, 2}};
  EXPECT_EQ(lookup.at(1), 2);
  List<int> list{1, 2};
  Deque<int> deque{3, 4};
  Queue<int> queue;
  Stack<int> stack;
  PriorityQueue<int> priority;
  queue.push(list.front());
  stack.push(deque.back());
  priority.push(9);
  EXPECT_EQ(queue.front(), 1);
  EXPECT_EQ(stack.top(), 4);
  EXPECT_EQ(priority.top(), 9);
  Set<int> unique{1, 1};
  MultiSet<int> repeated{1, 1};
  MultiMap<int, int> pairs{{1, 2}, {1, 3}};
  UnorderedSet<int> hashed{1, 2};
  UnorderedMultiSet<int> hashed_repeated{1, 1};
  UnorderedMultiMap<int, int> hashed_pairs{{1, 2}, {1, 3}};
  EXPECT_EQ(unique.size(), 1U);
  EXPECT_EQ(repeated.size(), 2U);
  EXPECT_EQ(pairs.size(), 2U);
  EXPECT_EQ(hashed.size(), 2U);
  EXPECT_EQ(hashed_repeated.size(), 2U);
  EXPECT_EQ(hashed_pairs.size(), 2U);
  auto pair = kitzoo::memory::make_pair(1, 2);
  EXPECT_EQ(pair.second, 2);
}

TEST(MemoryTypesTest, SharedFactorySupportsWeakReferencesAndAlignedObjects) {
  namespace memory = kitzoo::memory;
  memory::WeakPtr<FactoryTracked> weak;
  {
    memory::SharedPtr<FactoryTracked> object = memory::make_shared<FactoryTracked>(42);
    weak = object;
    auto locked = weak.lock();
    ASSERT_NE(locked, nullptr);
    EXPECT_EQ(locked.get(), object.get());
    EXPECT_EQ(FactoryTracked::live_, 1);
    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(object.get()) % alignof(FactoryTracked), 0U);
    object.reset();
    EXPECT_EQ(FactoryTracked::live_, 1);
  }
  EXPECT_EQ(FactoryTracked::live_, 0);
  EXPECT_TRUE(weak.expired());
  EXPECT_FALSE(weak.lock());
  EXPECT_THROW(static_cast<void>(memory::make_shared<FactoryThrowing>(1)), std::runtime_error);
}

TEST(MemorySharedPtrTest, DefaultFactorySupportsArrays) {
  auto values = kitzoo::memory::make_shared<int[]>(std::size_t{3});
  EXPECT_EQ(values[0], 0);
  values[2] = 42;
  auto copied = values;
  values.reset();
  EXPECT_EQ(copied[2], 42);
}

#ifdef KITZOO_MIMALLOC_OVERRIDE
TEST(MemoryOverrideTest, ProcessAllocationsComeFromMimalloc) {
  void* raw = std::malloc(64);
  auto* object = new int(7);
  const std::string text(256, 'x');
  const std::vector<int> values(1024, 1);
  EXPECT_TRUE(mi_is_in_heap_region(raw));
  EXPECT_TRUE(mi_is_in_heap_region(object));
  EXPECT_TRUE(mi_is_in_heap_region(text.data()));
  EXPECT_TRUE(mi_is_in_heap_region(values.data()));
  // Exit-time teardown stays off so late shared-library destructors can allocate.
  EXPECT_EQ(mi_option_get(mi_option_destroy_on_exit), 2);
  delete object;
  std::free(raw);
}
#endif
