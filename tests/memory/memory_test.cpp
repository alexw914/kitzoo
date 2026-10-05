// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: tests/memory/memory_test.cpp
// Description: Verifies container aliases, ownership, alignment, and construction rollback.
// -----------------------------------------------------------------------------

#include <kitzoo/memory/memory.hpp>
#include <kitzoo/memory/resource.hpp>

#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <limits>
#include <memory>
#include <stdexcept>
#include <type_traits>

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

// Public container aliases and smart pointer factories.
namespace {

struct alignas(256) FactoryTracked {
  static inline int live = 0;
  int value;

  explicit FactoryTracked(int input) : value(input) { ++live; }

  ~FactoryTracked() { --live; }
};

struct FactoryThrowing {
  explicit FactoryThrowing(int) { throw std::runtime_error("constructor failed"); }
};

} // namespace

TEST(MemoryUniquePtrTest, ConstructionMoveResetAndExplicitRelease) {
  namespace memory = kitzoo::memory;
  static_assert(std::is_empty_v<memory::MiDeleter<FactoryTracked>>);
  static_assert(!std::is_copy_constructible_v<memory::UniquePtr<FactoryTracked>>);
  static_assert(std::is_nothrow_move_constructible_v<memory::UniquePtr<FactoryTracked>>);
  static_assert(std::is_same_v<decltype(memory::make_unique<FactoryTracked>(42)), memory::UniquePtr<FactoryTracked>>);
  {
    auto object = memory::make_unique<FactoryTracked>(42);
    EXPECT_EQ(object->value, 42);
    EXPECT_EQ(FactoryTracked::live, 1);
    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(object.get()) % 256, 0U);
    auto moved = std::move(object);
    EXPECT_FALSE(object);
    auto* raw = moved.release();
    EXPECT_FALSE(moved);
    memory::UniquePtr<FactoryTracked> adopted{raw};
    auto replacement = memory::make_unique<FactoryTracked>(7);
    adopted.reset(replacement.release());
    EXPECT_FALSE(replacement);
    EXPECT_EQ(FactoryTracked::live, 1);
    EXPECT_EQ(adopted->value, 7);
    adopted.reset();
    EXPECT_EQ(FactoryTracked::live, 0);
  }
  EXPECT_EQ(FactoryTracked::live, 0);
  EXPECT_EQ(*memory::make_unique<int>(), 0);
  auto text = memory::make_unique<memory::String>(100, 'x');
  EXPECT_EQ(text->size(), 100U);
  std::unique_ptr<int> standard = std::make_unique<int>(9);
  EXPECT_EQ(*standard, 9);
}

TEST(MemoryUniquePtrTest, ConstructorExceptionPropagates) {
  EXPECT_THROW(kitzoo::memory::make_unique<FactoryThrowing>(1), std::runtime_error);
  kitzoo::memory::MiDeleter<int>{}(nullptr);
}

TEST(MemoryUniquePtrTest, FactorySupportsCustomDefaultDeleter) {
  struct Deleter : kitzoo::memory::MiDeleter<FactoryTracked> {};

  auto object = kitzoo::memory::make_unique<FactoryTracked, Deleter>(42);
  static_assert(std::is_same_v<decltype(object), kitzoo::memory::UniquePtr<FactoryTracked, Deleter>>);
  EXPECT_EQ(object->value, 42);
  EXPECT_EQ(FactoryTracked::live, 1);
  object.reset();
  EXPECT_EQ(FactoryTracked::live, 0);
}

TEST(MemoryUniquePtrTest, StatefulDeleterSurvivesMoveAndRunsOnce) {
  struct Deleter {
    int* calls;
    kitzoo::memory::UniquePtr<int> state;

    auto operator()(FactoryTracked* object) const noexcept -> void {
      ++*calls;
      kitzoo::memory::MiDeleter<FactoryTracked>{}(object);
    }
  };

  int calls = 0;
  static_assert(!std::is_copy_constructible_v<Deleter>);
  auto object = kitzoo::memory::make_unique_with_deleter<FactoryTracked>(
      Deleter{&calls, kitzoo::memory::make_unique<int>(7)}, 42);
  EXPECT_EQ(object.get_deleter().calls, &calls);
  auto moved = std::move(object);
  EXPECT_FALSE(object);
  EXPECT_EQ(moved->value, 42);
  EXPECT_EQ(*moved.get_deleter().state, 7);
  moved.reset();
  EXPECT_EQ(calls, 1);
  EXPECT_EQ(FactoryTracked::live, 0);
  moved.reset();
  EXPECT_EQ(calls, 1);

  auto destroy_throwing = [&calls](FactoryThrowing* value) noexcept -> void {
    ++calls;
    kitzoo::memory::MiDeleter<FactoryThrowing>{}(value);
  };
  EXPECT_THROW(kitzoo::memory::make_unique_with_deleter<FactoryThrowing>(destroy_throwing, 1), std::runtime_error);
  EXPECT_EQ(calls, 1);
}

TEST(MemoryTypesTest, ContainerAliases) {
  using namespace kitzoo::memory;
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
    EXPECT_EQ(FactoryTracked::live, 1);
    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(object.get()) % alignof(FactoryTracked), 0U);
    object.reset();
    EXPECT_EQ(FactoryTracked::live, 1);
  }
  EXPECT_EQ(FactoryTracked::live, 0);
  EXPECT_TRUE(weak.expired());
  EXPECT_FALSE(weak.lock());
  EXPECT_THROW(memory::make_shared<FactoryThrowing>(1), std::runtime_error);
}

TEST(MemorySharedPtrTest, StatefulDeleterRunsAfterLastOwner) {
  namespace memory = kitzoo::memory;

  struct Deleter {
    int* calls;
    memory::UniquePtr<int> state;

    auto operator()(FactoryTracked* object) const noexcept -> void {
      ++*calls;
      memory::MiDeleter<FactoryTracked>{}(object);
    }
  };

  static_assert(!std::is_copy_constructible_v<Deleter>);
  int calls = 0;
  auto first = memory::make_shared_with_deleter<FactoryTracked>(Deleter{&calls, memory::make_unique<int>(7)}, 42);
  static_assert(std::is_same_v<decltype(first), memory::SharedPtr<FactoryTracked>>);
  ASSERT_EQ(FactoryTracked::live, 1);
  EXPECT_EQ(first->value, 42);
  EXPECT_EQ(reinterpret_cast<std::uintptr_t>(first.get()) % alignof(FactoryTracked), 0U);
  auto* stored = std::get_deleter<Deleter>(first);
  ASSERT_NE(stored, nullptr);
  EXPECT_EQ(*stored->state, 7);

  auto second = first;
  memory::WeakPtr<FactoryTracked> weak = first;
  first.reset();
  EXPECT_EQ(calls, 0);
  EXPECT_EQ(FactoryTracked::live, 1);
  second.reset();
  EXPECT_EQ(calls, 1);
  EXPECT_EQ(FactoryTracked::live, 0);
  EXPECT_TRUE(weak.expired());
  weak.reset();
  EXPECT_EQ(calls, 1);
}

TEST(MemorySharedPtrTest, ConstructorFailureDoesNotInvokeDeleter) {
  int calls = 0;
  auto deleter = [&calls](FactoryThrowing* object) noexcept -> void {
    ++calls;
    kitzoo::memory::MiDeleter<FactoryThrowing>{}(object);
  };

  EXPECT_THROW(kitzoo::memory::make_shared_with_deleter<FactoryThrowing>(deleter, 1), std::runtime_error);
  EXPECT_EQ(calls, 0);
}

TEST(MemorySharedPtrTest, FactorySupportsCustomDefaultDeleter) {
  namespace memory = kitzoo::memory;

  struct Deleter : memory::MiDeleter<FactoryTracked> {};

  auto object = memory::make_shared<FactoryTracked, Deleter>(42);
  EXPECT_EQ(object->value, 42);
  EXPECT_EQ(FactoryTracked::live, 1);
  EXPECT_NE(std::get_deleter<Deleter>(object), nullptr);
  object.reset();
  EXPECT_EQ(FactoryTracked::live, 0);
}

TEST(MemorySharedPtrTest, DefaultFactorySupportsArrays) {
  auto values = kitzoo::memory::make_shared<int[]>(std::size_t{3});
  EXPECT_EQ(values[0], 0);
  values[2] = 42;
  auto copied = values;
  values.reset();
  EXPECT_EQ(copied[2], 42);
}
