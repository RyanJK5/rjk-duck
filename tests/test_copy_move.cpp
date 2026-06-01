#include "duck.hpp"
#include "test_fixtures.hpp"

#include <gtest/gtest.h>

namespace rjk_test {
TEST(DuckCopy, CopyConstruct) {
    TestDuck x{A{}};
    TestDuck y{x};
    EXPECT_TRUE(y.has_value());
    EXPECT_EQ(y.other('a'), 10);
}

TEST(DuckCopy, CopyConstructIndependent) {
    TestDuck x{A{}};
    TestDuck y{x};
    x = B{};
    EXPECT_EQ(y.other('a'), 10); // y unaffected
    EXPECT_EQ(x.other('a'), 3);
}

TEST(DuckCopy, CopyAssign) {
    TestDuck x{A{}};
    TestDuck y{B{}};
    y = x;
    EXPECT_EQ(y.other('a'), 10);
}

TEST(DuckCopy, CopyAssignIndependent) {
    TestDuck x{A{}};
    TestDuck y{B{}};
    y = x;
    x = B{};
    EXPECT_EQ(y.other('a'), 10); // y unaffected
}

TEST(DuckCopy, CopyAssignSelf) {
    TestDuck x{A{}};
    x = static_cast<const TestDuck&>(x); // self-assign
    EXPECT_TRUE(x.has_value());
    EXPECT_EQ(x.other('a'), 10);
}

TEST(DuckCopy, CopyHeap) {
    TestDuck x{Big{}};
    TestDuck y{x};
    EXPECT_EQ(y.other('a'), 99);
}

TEST(DuckMove, MoveConstructSBO) {
    TestDuck x{A{}};
    TestDuck y{std::move(x)};
    EXPECT_TRUE(y.has_value());
    EXPECT_EQ(y.other('a'), 10);
    EXPECT_FALSE(x.has_value());
}

TEST(DuckMove, MoveConstructHeap) {
    TestDuck x{Big{}};
    TestDuck y{std::move(x)};
    EXPECT_TRUE(y.has_value());
    EXPECT_EQ(y.other('a'), 99);
    EXPECT_FALSE(x.has_value());
}

TEST(DuckMove, MoveAssignSBO) {
    TestDuck x{A{}};
    TestDuck y{B{}};
    y = std::move(x);
    EXPECT_TRUE(y.has_value());
    EXPECT_EQ(y.other('a'), 10);
    EXPECT_FALSE(x.has_value());
}

TEST(DuckMove, MoveAssignHeap) {
    TestDuck x{Big{}};
    TestDuck y{B{}};
    y = std::move(x);
    EXPECT_TRUE(y.has_value());
    EXPECT_EQ(y.other('a'), 99);
    EXPECT_FALSE(x.has_value());
}

TEST(DuckMove, MoveAssignSelf) {
    TestDuck x{A{}};
    TestDuck& alias = x;

    x = std::move(alias);
    // post self-move state is valid but unspecified; just don't crash
}

TEST(DuckMove, MoveOnlyType) {
    rjk::duck<rjk::policy<
        rjk::has_fn<"test", void()>,
        rjk::has_fn<"other", int(char)>
    >> x{MoveOnly{7}};
    EXPECT_EQ(x.other('a'), 7);
    auto y = std::move(x);
    EXPECT_EQ(y.other('a'), 7);
    EXPECT_FALSE(x.has_value());
}
}
