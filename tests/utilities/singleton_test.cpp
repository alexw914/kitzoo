// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: tests/utilities/singleton_test.cpp
// Description: Verifies singleton identity, startup access and concurrent initialization.
// -----------------------------------------------------------------------------

#include <kitzoo/utilities/singleton.hpp>

#include <array>
#include <atomic>
#include <barrier>
#include <cstddef>
#include <gtest/gtest.h>
#include <thread>
#include <type_traits>
#include <vector>

namespace {

template <typename Tag>
struct Service {
    Service() { constructions.fetch_add(1); }

    static inline std::atomic<int> constructions{0};
    int value{42};
};

struct LazyTag {};

struct EagerTag {};

using LazyService = Service<LazyTag>;
using EagerService = Service<EagerTag>;

// This access can precede the eager anchor's dynamic initialization.
const int kStartupValue = kitzoo::util::EagerSingleton<EagerService>::instance().value;

class DerivedService : public kitzoo::util::Singleton<DerivedService> {
    friend class kitzoo::util::Singleton<DerivedService>;
    DerivedService() = default;
};

static_assert(!std::is_default_constructible_v<DerivedService>);
static_assert(!std::is_copy_constructible_v<DerivedService>);
static_assert(!std::is_move_constructible_v<DerivedService>);
static_assert(!std::is_copy_assignable_v<DerivedService>);
static_assert(!std::is_move_assignable_v<DerivedService>);

class EagerDerivedService : public kitzoo::util::EagerSingleton<EagerDerivedService> {
    friend class kitzoo::util::EagerSingleton<EagerDerivedService>;
    EagerDerivedService() = default;
};

static_assert(!std::is_default_constructible_v<EagerDerivedService>);
static_assert(!std::is_copy_constructible_v<EagerDerivedService>);
static_assert(!std::is_move_constructible_v<EagerDerivedService>);

template <typename Wrapper, typename Value>
auto verify_concurrent_access() -> void {
    constexpr std::size_t kThreads = 16;
    std::array<Value*, kThreads> instances{};
    std::barrier start{static_cast<std::ptrdiff_t>(kThreads)};
    std::vector<std::jthread> threads;
    for (std::size_t i = 0; i < kThreads; ++i) {
        threads.emplace_back([&, i]() -> void {
            start.arrive_and_wait();
            instances[i] = &Wrapper::instance();
        });
    }
    threads.clear();
    ASSERT_NE(instances.front(), nullptr);
    for (auto* instance : instances) {
        EXPECT_EQ(instance, instances.front());
        EXPECT_EQ(instance->value, 42);
    }
    EXPECT_EQ(Value::constructions.load(), 1);
}

}  // namespace

TEST(SingletonTest, DerivedClassUsesInheritedInstance) {
    EXPECT_EQ(&DerivedService::instance(), &DerivedService::instance());
}

TEST(SingletonTest, ConcurrentLazyAccessConstructsOnce) {
    verify_concurrent_access<kitzoo::util::Singleton<LazyService>, LazyService>();
}

TEST(SingletonTest, ReturnsOneEagerlyInitializedInstance) {
    EXPECT_EQ(kStartupValue, 42);
    verify_concurrent_access<kitzoo::util::EagerSingleton<EagerService>, EagerService>();
    EXPECT_EQ(&EagerDerivedService::instance(), &EagerDerivedService::instance());
}
