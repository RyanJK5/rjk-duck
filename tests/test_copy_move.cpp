#include "duck.hpp"
#include "test_fixtures.hpp"

#include <gtest/gtest.h>

namespace rjk_test {
TEST(AnyCopy, CopyConstruct) {
    TestAny x{A{}};
    TestAny y{x};
    EXPECT_TRUE(y.has_value());
    EXPECT_EQ(y->other('a'), 10);
}

TEST(AnyCopy, CopyConstructIndependent) {
    TestAny x{A{}};
    TestAny y{x};
    x = B{};
    EXPECT_EQ(y->other('a'), 10); // y unaffected
    EXPECT_EQ(x->other('a'), 3);
}

TEST(AnyCopy, CopyAssign) {
    TestAny x{A{}};
    TestAny y{B{}};
    y = x;
    EXPECT_EQ(y->other('a'), 10);
}

TEST(AnyCopy, CopyAssignIndependent) {
    TestAny x{A{}};
    TestAny y{B{}};
    y = x;
    x = B{};
    EXPECT_EQ(y->other('a'), 10); // y unaffected
}

TEST(AnyCopy, CopyAssignSelf) {
    TestAny x{A{}};
    x = static_cast<const TestAny&>(x); // self-assign
    EXPECT_TRUE(x.has_value());
    EXPECT_EQ(x->other('a'), 10);
}

TEST(AnyCopy, CopyHeap) {
    TestAny x{Big{}};
    TestAny y{x};
    EXPECT_EQ(y->other('a'), 99);
}

TEST(AnyMove, MoveConstructSBO) {
    TestAny x{A{}};
    TestAny y{std::move(x)};
    EXPECT_TRUE(y.has_value());
    EXPECT_EQ(y->other('a'), 10);
    EXPECT_FALSE(x.has_value());
}

TEST(AnyMove, MoveConstructHeap) {
    TestAny x{Big{}};
    TestAny y{std::move(x)};
    EXPECT_TRUE(y.has_value());
    EXPECT_EQ(y->other('a'), 99);
    EXPECT_FALSE(x.has_value());
}

TEST(AnyMove, MoveAssignSBO) {
    TestAny x{A{}};
    TestAny y{B{}};
    y = std::move(x);
    EXPECT_TRUE(y.has_value());
    EXPECT_EQ(y->other('a'), 10);
    EXPECT_FALSE(x.has_value());
}

TEST(AnyMove, MoveAssignHeap) {
    TestAny x{Big{}};
    TestAny y{B{}};
    y = std::move(x);
    EXPECT_TRUE(y.has_value());
    EXPECT_EQ(y->other('a'), 99);
    EXPECT_FALSE(x.has_value());
}

TEST(AnyMove, MoveAssignSelf) {
    TestAny x{A{}};
    TestAny& alias = x;

    x = std::move(alias);
    // post self-move state is valid but unspecified; just don't crash
}

TEST(AnyMove, MoveOnlyType) {
    rjk::duck<rjk::has_fn<"test", void()>, rjk::has_fn<"other", int(char)> > x{
        MoveOnly{7}};
    EXPECT_EQ(x->other('a'), 7);
    auto y = std::move(x);
    EXPECT_EQ(y->other('a'), 7);
    EXPECT_FALSE(x.has_value());
}
}
