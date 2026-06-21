#include "rjk/duck.hpp"
#include "test_fixtures.hpp"

#include <gtest/gtest.h>

namespace rjk_test {
TEST(DuckLifetime, DestructorCalledOnDestroy) {
    A::instance_count = 0;
    {
        TestDuck x{A{}};
        EXPECT_EQ(A::instance_count, 1);
    }
    EXPECT_EQ(A::instance_count, 0);
}

TEST(DuckLifetime, DestructorCalledOnReassign) {
    A::instance_count = 0;
    TestDuck x{A{}};
    EXPECT_EQ(A::instance_count, 1);
    x = B{};
    EXPECT_EQ(A::instance_count, 0);
}

TEST(DuckLifetime, DestructorCalledOnEmplace) {
    A::instance_count = 0;
    TestDuck x{A{}};
    EXPECT_EQ(A::instance_count, 1);
    x.emplace<B>();
    EXPECT_EQ(A::instance_count, 0);
}

TEST(DuckLifetime, HeapDestructorCalled) {
    // Exercises the heap delete path
    TestDuck x{Big{}};
}
}
