#include "duck.hpp"
#include "test_fixtures.hpp"

#include <gtest/gtest.h>

namespace rjk_test {
TEST(DuckConstruct, DefaultConstruct) {
    TestDuck x;
    EXPECT_FALSE(x.has_value());
}

TEST(DuckConstruct, FromRvalue) {
    TestDuck x{A{}};
    EXPECT_TRUE(x.has_value());
    EXPECT_NO_THROW(x->test());
    EXPECT_EQ(x->other('a'), 10);
}

TEST(DuckConstruct, FromLvalue) {
    A a{};
    TestDuck x{a};
    EXPECT_TRUE(x.has_value());
    EXPECT_EQ(x->other('a'), 10);
}

TEST(DuckConstruct, InPlaceType) {
    TestDuck x{std::in_place_type<A>};
    EXPECT_TRUE(x.has_value());
    EXPECT_EQ(x->other('a'), 10);
}

TEST(DuckConstruct, InPlaceTypeWithArgs) {
    // B has no constructor args but exercises the path
    TestDuck x{std::in_place_type<B>};
    EXPECT_TRUE(x.has_value());
    EXPECT_EQ(x->other('a'), 3);
}

TEST(DuckConstruct, InPlaceTypeInitializerList) {
    // Use a type constructible from initializer_list
    struct FromIL {
        int sum = 0;

        FromIL(std::initializer_list<int> il) {
            for (int v : il)
                sum += v;
        }

        void test() {}

        int other(char) { return sum; }
    };
    TestDuck x{std::in_place_type<FromIL>, {1, 2, 3}};
    EXPECT_EQ(x->other('a'), 6);
}

TEST(DuckConstruct, HeapAllocated) {
    TestDuck x{Big{}};
    EXPECT_TRUE(x.has_value());
    EXPECT_EQ(x->other('a'), 99);
    // Big is 64 bytes > SBO size of 32, so heap path is taken
    EXPECT_FALSE(rjk::detail::fits_sbo<Big>);
}

TEST(DuckAssign, AssignRvalue) {
    TestDuck x{B{}};
    x = A{};
    EXPECT_EQ(x->other('a'), 10);
    x = B{};
    EXPECT_EQ(x->other('a'), 3);
}

TEST(DuckAssign, AssignLvalue) {
    TestDuck x{B{}};
    A a{};
    x = a;
    EXPECT_EQ(x->other('a'), 10);
}

TEST(DuckAssign, AssignHeap) {
    TestDuck x{A{}};
    x = Big{};
    EXPECT_EQ(x->other('a'), 99);
}
}
