// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: tests/core/singleton_test.cpp
// Description: Verifies lazy and eager singleton construction, startup access,
//              initialization retries, and concurrent access.
// -----------------------------------------------------------------------------

#include <kitzoo/core/singleton.hpp>

#include <array>
#include <atomic>
#include <barrier>
#include <cstddef>
#include <gtest/gtest.h>
#include <stdexcept>
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

struct ConcurrentTag {};

struct EagerTag {};

struct StartupTag {};

using LazyService = Service<LazyTag>;
using ConcurrentService = Service<ConcurrentTag>;
using EagerService = Service<EagerTag>;
using StartupService = Service<StartupTag>;

// This call may precede the eager initializer and must see a complete object.
const int kStartupValue = kitzoo::core::EagerSingleton<StartupService>::instance().value;

class DerivedService : public kitzoo::core::Singleton<DerivedService> {
  friend class kitzoo::core::Singleton<DerivedService>;
  DerivedService() = default;
};

static_assert(!std::is_default_constructible_v<DerivedService>);
static_assert(!std::is_copy_constructible_v<DerivedService>);
static_assert(!std::is_move_constructible_v<DerivedService>);
static_assert(!std::is_copy_assignable_v<DerivedService>);
static_assert(!std::is_move_assignable_v<DerivedService>);

class EagerDerivedService : public kitzoo::core::EagerSingleton<EagerDerivedService> {
  friend class kitzoo::core::EagerSingleton<EagerDerivedService>;
  EagerDerivedService() = default;
};

static_assert(!std::is_default_constructible_v<EagerDerivedService>);
static_assert(!std::is_copy_constructible_v<EagerDerivedService>);
static_assert(!std::is_move_constructible_v<EagerDerivedService>);
static_assert(!std::is_copy_assignable_v<EagerDerivedService>);
static_assert(!std::is_move_assignable_v<EagerDerivedService>);

class RetryService : public kitzoo::core::Singleton<RetryService> {
  friend class kitzoo::core::Singleton<RetryService>;

public:
  static inline std::atomic<int> attempts{0};
  int value{42};

private:
  RetryService() {
    if (attempts.fetch_add(1) == 0)
      throw std::runtime_error{"first initialization fails"};
  }
};

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

} // namespace

TEST(SingletonTest, DerivedClassUsesInheritedInstance) {
  EXPECT_EQ(&DerivedService::instance(), &DerivedService::instance());
}

TEST(SingletonTest, ConcurrentLazyAccessConstructsOnce) {
  verify_concurrent_access<kitzoo::core::Singleton<ConcurrentService>, ConcurrentService>();
}

TEST(SingletonTest, ConstructsOnlyOnFirstAccess) {
  EXPECT_EQ(LazyService::constructions.load(), 0);
  auto& instance = kitzoo::core::Singleton<LazyService>::instance();
  EXPECT_EQ(instance.value, 42);
  EXPECT_EQ(LazyService::constructions.load(), 1);
  EXPECT_EQ(&instance, &kitzoo::core::Singleton<LazyService>::instance());
  EXPECT_EQ(LazyService::constructions.load(), 1);
}

TEST(SingletonTest, RetriesAfterConstructorThrows) {
  EXPECT_THROW((void)RetryService::instance(), std::runtime_error);
  EXPECT_EQ(RetryService::attempts.load(), 1);
  auto& instance = RetryService::instance();
  EXPECT_EQ(instance.value, 42);
  EXPECT_EQ(RetryService::attempts.load(), 2);
  EXPECT_EQ(&instance, &RetryService::instance());
  EXPECT_EQ(RetryService::attempts.load(), 2);
}

TEST(SingletonTest, EagerDerivedClassUsesInheritedInstance) {
  EXPECT_EQ(&EagerDerivedService::instance(), &EagerDerivedService::instance());
}

TEST(SingletonTest, ConcurrentEagerAccessConstructsOnce) {
  verify_concurrent_access<kitzoo::core::EagerSingleton<EagerService>, EagerService>();
}

TEST(SingletonTest, EagerAccessDuringStaticStartupIsSafe) {
  EXPECT_EQ(kStartupValue, 42);
  verify_concurrent_access<kitzoo::core::EagerSingleton<StartupService>, StartupService>();
}
