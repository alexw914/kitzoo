// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: tests/memory/advanced_types_test.cpp
// Description: Verifies mimalloc container aliases, shared ownership and unique ownership.
// -----------------------------------------------------------------------------

#include <kitzoo/memory/advanced_types.hpp>

#include <cstdint>
#include <gtest/gtest.h>
#include <stdexcept>
#include <type_traits>

namespace {

struct alignas(256) Tracked {
    static inline int live = 0;
    int value;

    explicit Tracked(int input) : value(input) { ++live; }

    ~Tracked() { --live; }
};

struct Throwing {
    explicit Throwing(int) { throw std::runtime_error("constructor failed"); }
};

}  // namespace

TEST(UniquePtrTest, ConstructionMoveResetAndExplicitRelease) {
    namespace memory = kitzoo::memory;
    static_assert(std::is_empty_v<memory::MiDeleter<Tracked>>);
    static_assert(!std::is_copy_constructible_v<memory::UniquePtr<Tracked>>);
    static_assert(std::is_nothrow_move_constructible_v<memory::UniquePtr<Tracked>>);
    static_assert(
        std::is_same_v<decltype(memory::make_unique<Tracked>(42)), memory::UniquePtr<Tracked>>);
    {
        auto object = memory::make_unique<Tracked>(42);
        EXPECT_EQ(object->value, 42);
        EXPECT_EQ(Tracked::live, 1);
        EXPECT_EQ(reinterpret_cast<std::uintptr_t>(object.get()) % 256, 0U);
        auto moved = std::move(object);
        EXPECT_FALSE(object);
        auto* raw = moved.release();
        EXPECT_FALSE(moved);
        memory::UniquePtr<Tracked> adopted{raw};
        auto replacement = memory::make_unique<Tracked>(7);
        adopted.reset(replacement.release());
        EXPECT_FALSE(replacement);
        EXPECT_EQ(Tracked::live, 1);
        EXPECT_EQ(adopted->value, 7);
        adopted.reset();
        EXPECT_EQ(Tracked::live, 0);
    }
    EXPECT_EQ(Tracked::live, 0);
    EXPECT_EQ(*memory::make_unique<int>(), 0);
    auto text = memory::make_unique<memory::String>(100, 'x');
    EXPECT_EQ(text->size(), 100U);
    std::unique_ptr<int> standard = std::make_unique<int>(9);
    EXPECT_EQ(*standard, 9);
}

TEST(UniquePtrTest, ConstructorExceptionPropagates) {
    EXPECT_THROW(kitzoo::memory::make_unique<Throwing>(1), std::runtime_error);
    kitzoo::memory::MiDeleter<int>{}(nullptr);
}

TEST(AdvancedTypesTest, ContainerAliases) {
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

TEST(AdvancedTypesTest, SharedFactorySupportsWeakReferencesAndAlignedObjects) {
    namespace memory = kitzoo::memory;
    memory::WeakPtr<Tracked> weak;
    {
        memory::SharedPtr<Tracked> object = memory::make_shared<Tracked>(42);
        weak = object;
        auto locked = weak.lock();
        ASSERT_NE(locked, nullptr);
        EXPECT_EQ(locked.get(), object.get());
        EXPECT_EQ(Tracked::live, 1);
        EXPECT_EQ(reinterpret_cast<std::uintptr_t>(object.get()) % alignof(Tracked), 0U);
        object.reset();
        EXPECT_EQ(Tracked::live, 1);
    }
    EXPECT_EQ(Tracked::live, 0);
    EXPECT_TRUE(weak.expired());
    EXPECT_FALSE(weak.lock());
    EXPECT_THROW(memory::make_shared<Throwing>(1), std::runtime_error);
}
