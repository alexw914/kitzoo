#include <kitzoo/utilities/singleton.hpp>

#include <atomic>
#include <gtest/gtest.h>
#include <type_traits>

namespace {

struct Service {
    Service() { ++constructions; }
    static inline std::atomic<int> constructions{0};
};

class DerivedService : public kitzoo::util::Singleton<DerivedService> {
    friend class kitzoo::util::Singleton<DerivedService>;
    DerivedService() = default;
};

static_assert(!std::is_default_constructible_v<DerivedService>);
static_assert(!std::is_copy_constructible_v<DerivedService>);
static_assert(!std::is_move_constructible_v<DerivedService>);
static_assert(!std::is_copy_assignable_v<DerivedService>);
static_assert(!std::is_move_assignable_v<DerivedService>);
static_assert(!std::is_copy_constructible_v<kitzoo::util::EagerSingleton<Service>>);
static_assert(!std::is_move_constructible_v<kitzoo::util::EagerSingleton<Service>>);

}  // namespace

TEST(SingletonTest, DerivedClassUsesInheritedInstance) {
    EXPECT_EQ(&DerivedService::instance(), &DerivedService::instance());
}

TEST(SingletonTest, ReturnsOneLazilyConstructedInstance) {
    Service::constructions = 0;

    auto* first = &kitzoo::util::Singleton<Service>::instance();
    auto* second = &kitzoo::util::Singleton<Service>::instance();

    EXPECT_EQ(first, second);
    EXPECT_EQ(Service::constructions.load(), 1);
}

TEST(SingletonTest, ReturnsOneEagerlyInitializedInstance) {
    auto* first = &kitzoo::util::EagerSingleton<Service>::instance();
    auto* second = &kitzoo::util::EagerSingleton<Service>::instance();

    EXPECT_EQ(first, second);
}
