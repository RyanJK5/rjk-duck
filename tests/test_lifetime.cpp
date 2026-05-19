#include "duck.hpp"
#include "test_fixtures.hpp"

#include <gtest/gtest.h>

namespace rjk_test {
TEST(AnyLifetime, DestructorCalledOnDestroy) {
    A::instance_count = 0;
    {
        TestAny x{A{}};
        EXPECT_EQ(A::instance_count, 1);
    }
    EXPECT_EQ(A::instance_count, 0);
}

TEST(AnyLifetime, DestructorCalledOnReassign) {
    A::instance_count = 0;
    TestAny x{A{}};
    EXPECT_EQ(A::instance_count, 1);
    x = B{};
    EXPECT_EQ(A::instance_count, 0);
}

TEST(AnyLifetime, DestructorCalledOnEmplace) {
    A::instance_count = 0;
    TestAny x{A{}};
    EXPECT_EQ(A::instance_count, 1);
    x.emplace<B>();
    EXPECT_EQ(A::instance_count, 0);
}

TEST(AnyLifetime, HeapDestructorCalled) {
    // Exercises the heap delete path
    {
        TestAny x{Big{}};
        EXPECT_TRUE(x.has_value());
    }
}

TEST(BadAnyAccess, WhatMessage) {
    rjk::bad_duck_access ex{};
    EXPECT_STREQ(ex.what(), "type mismatch");
}
}
