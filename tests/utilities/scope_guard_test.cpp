// ---------------------------------------------------------------------------
// kitzoo/utilities ScopeGuard tests
// ---------------------------------------------------------------------------

#include <kitzoo/utilities/scope_guard.hpp>

#include <gtest/gtest.h>
#include <stdexcept>

using namespace kitzoo::util;

TEST(ScopeGuardTest, ExecutesOnScopeExit) {
    bool ran = false;
    {
        auto guard = make_scope_guard([&] { ran = true; });
        EXPECT_TRUE(guard.active());
    }
    EXPECT_TRUE(ran);
}

TEST(ScopeGuardTest, DismissPreventsExecution) {
    bool ran = false;
    {
        auto guard = make_scope_guard([&] { ran = true; });
        guard.dismiss();
        EXPECT_FALSE(guard.active());
    }
    EXPECT_FALSE(ran);
}

TEST(ScopeGuardTest, MoveTransfersOwnership) {
    int count = 0;
    {
        auto g1 = make_scope_guard([&] { ++count; });
        auto g2 = std::move(g1);
        EXPECT_FALSE(g1.active());  // source disarmed
        EXPECT_TRUE(g2.active());
    }
    EXPECT_EQ(count, 1);  // exactly once
}

TEST(ScopeGuardTest, ExecutesDuringExceptionUnwinding) {
    bool ran = false;
    auto throwing_fn = [&] {
        auto guard = make_scope_guard([&] { ran = true; });
        throw std::runtime_error{"boom"};
    };
    EXPECT_THROW(throwing_fn(), std::runtime_error);
    EXPECT_TRUE(ran);
}

TEST(ScopeGuardTest, ExactlyOnceAfterMoveChain) {
    int count = 0;
    {
        auto g1 = make_scope_guard([&] { ++count; });
        auto g2 = std::move(g1);
        auto g3 = std::move(g2);
    }
    EXPECT_EQ(count, 1);
}

TEST(ScopeGuardTest, DeductionGuide) {
    bool ran = false;
    {
        ScopeGuard guard{[&] { ran = true; }};  // CTAD, no factory
    }
    EXPECT_TRUE(ran);
}

TEST(ScopeGuardTest, ValueCaptureByMove) {
    std::string resource = "owned";
    bool saw_owned = false;
    {
        auto guard =
            make_scope_guard([r = std::move(resource), &saw_owned] { saw_owned = (r == "owned"); });
        EXPECT_TRUE(resource.empty());  // moved-from
    }
    EXPECT_TRUE(saw_owned);
}
