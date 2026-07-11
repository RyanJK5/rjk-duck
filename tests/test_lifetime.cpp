#include "rjk/duck.hpp"

#include <gtest/gtest.h>

namespace rjk_test {

// ============================================================
// Instance-counting types
// ============================================================

struct [[=rjk::trait]] Tracked {
    auto tag() const -> int;
};

// Small enough to live in the SBO buffer.
struct Small {
    inline static thread_local int instance_count = 0;

    Small() { ++instance_count; }
    Small(const Small&) { ++instance_count; }
    Small(Small&&) noexcept { ++instance_count; }
    ~Small() { --instance_count; }
    Small& operator=(const Small&) = default;
    Small& operator=(Small&&) = default;

    auto tag() const -> int { return 1; }
};

// Larger than the default SBO buffer, forces heap allocation.
struct Big {
    inline static thread_local int instance_count = 0;

    std::array<std::byte, 64> padding{};

    Big() { ++instance_count; }
    Big(const Big&) { ++instance_count; }
    Big(Big&&) noexcept { ++instance_count; }
    ~Big() { --instance_count; }
    Big& operator=(const Big&) = default;
    Big& operator=(Big&&) = default;

    auto tag() const -> int { return 2; }
};

// Untracked type used purely as a reassignment target.
struct Other {
    auto tag() const -> int { return 0; }
};

using TrackedDuck = rjk::duck<Tracked, rjk::copyable>;

TEST(DuckLifetime, DestructorRunsWhenDuckGoesOutOfScopeSBO) {
    Small::instance_count = 0;
    {
        TrackedDuck d{Small{}};
        EXPECT_EQ(Small::instance_count, 1);
    }
    EXPECT_EQ(Small::instance_count, 0);
}

TEST(DuckLifetime, DestructorRunsWhenDuckGoesOutOfScopeHeap) {
    Big::instance_count = 0;
    {
        TrackedDuck d{Big{}};
        EXPECT_EQ(Big::instance_count, 1);
    }
    EXPECT_EQ(Big::instance_count, 0);
}

TEST(DuckLifetime, ReassignmentDestroysThePreviousValueSBO) {
    Small::instance_count = 0;
    TrackedDuck d{Small{}};
    EXPECT_EQ(Small::instance_count, 1);
    d = Other{};
    EXPECT_EQ(Small::instance_count, 0);
}

TEST(DuckLifetime, ReassignmentDestroysThePreviousValueHeap) {
    Big::instance_count = 0;
    TrackedDuck d{Big{}};
    EXPECT_EQ(Big::instance_count, 1);
    d = Other{};
    EXPECT_EQ(Big::instance_count, 0);
}

TEST(DuckLifetime, EmplaceDestroysThePreviousValue) {
    Small::instance_count = 0;
    TrackedDuck d{Small{}};
    EXPECT_EQ(Small::instance_count, 1);
    rjk::emplace<Other>(d);
    EXPECT_EQ(Small::instance_count, 0);
}

TEST(DuckLifetime, MoveConstructionTransfersOwnershipSBO) {
    Small::instance_count = 0;
    TrackedDuck x{Small{}};
    EXPECT_EQ(Small::instance_count, 1);
    TrackedDuck y{std::move(x)};
    EXPECT_EQ(Small::instance_count, 1);
}

TEST(DuckLifetime, MoveConstructionTransfersOwnershipHeap) {
    Big::instance_count = 0;
    TrackedDuck x{Big{}};
    EXPECT_EQ(Big::instance_count, 1);
    TrackedDuck y{std::move(x)};
    EXPECT_EQ(Big::instance_count, 1);
}

TEST(DuckLifetime, DestroyingAMovedFromDuckDoesNotDoubleDestroy) {
    Small::instance_count = 0;
    TrackedDuck x{Small{}};
    {
        TrackedDuck y{std::move(x)};
        EXPECT_EQ(Small::instance_count, 1);
    }
    // y's destructor already ran above; x going out of scope below must not
    // decrement the count a second time, since ownership was transferred.
    EXPECT_EQ(Small::instance_count, 0);
}

TEST(DuckLifetime, DestroyingACopyDoesNotAffectTheOriginal) {
    Small::instance_count = 0;
    TrackedDuck x{Small{}};
    {
        TrackedDuck y{x};
        EXPECT_EQ(Small::instance_count, 2);
    }
    EXPECT_EQ(Small::instance_count, 1);
}

TEST(DuckLifetime, SwapDoesNotChangeTotalInstanceCount) {
    Small::instance_count = 0;
    Big::instance_count = 0;
    TrackedDuck x{Small{}};
    TrackedDuck y{Big{}};
    std::swap(x, y);
    EXPECT_EQ(Small::instance_count, 1);
    EXPECT_EQ(Big::instance_count, 1);
}

TEST(DuckLifetime, SelfMoveAssignmentDoesNotLeak) {
    Small::instance_count = 0;
    TrackedDuck x{Small{}};
    TrackedDuck& alias = x;

    x = std::move(alias);
    EXPECT_EQ(Small::instance_count, 1);
}

}