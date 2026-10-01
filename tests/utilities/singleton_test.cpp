#include <kitzoo/utilities/singleton.hpp>

#include <atomic>
#include <gtest/gtest.h>

namespace {

struct Service {
    Service() { ++constructions; }
    static inline std::atomic<int> constructions{0};
};

}  // namespace

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
