// clang-format off
#include "rjk/duck.hpp"

#include <gtest/gtest.h>
#include <vector>
#include <string>

namespace rjk_test {

TEST(CallOperator, MultipleArgs) {
    struct Callable {
        int operator()(int, int) const;
    };

    struct Adder {
        int operator()(int a, int b) const { return a + b; }
    };

    struct Multiplier {
        int operator()(int a, int b) const { return a * b; }
    };

    rjk::duck<Callable> x{Adder{}};
    EXPECT_EQ(x(3, 4), 7);

    x = Multiplier{};
    EXPECT_EQ(x(3, 4), 12);
}

TEST(CallOperator, ThreeArgs) {
    struct Callable {
        int operator()(int, int, int) const;
    };

    struct Accumulator {
        int operator()(int a, int b, int c) const { return a + b + c; }
    };

    const rjk::duck<Callable> x{Accumulator{}};
    EXPECT_EQ(x(1, 2, 3), 6);
}

TEST(CallOperator, ConstAndNonConstOverload) {
    struct Callable {
        int operator()();
        int operator()() const;
    };

    struct Stateful {
        int calls = 0;
        int operator()()       { return ++calls; }
        int operator()() const { return 0; }
    };

    rjk::duck<Callable> x{Stateful{}};
    const rjk::duck<Callable> cx{Stateful{}};

    EXPECT_EQ(x(),  1);
    EXPECT_EQ(x(),  2);
    EXPECT_EQ(cx(), 0);
}

int multiply(int a, int b) {
    return a * b;
}

TEST(SubscriptOperator, CStyleArray) {
    struct Subscriptable {
        int& operator[](int index);
    };

    int x[5] = {1,2,3,4,5};
    rjk::duck<Subscriptable> d{x};
    EXPECT_EQ(d[0], x[0]);
    EXPECT_EQ(d[3], x[3]);
}

TEST(SubscriptOperator, Basic) {
    struct Subscriptable {
        int operator[](std::size_t) const;
    };

    struct Array {
        int data[4] = {10, 20, 30, 40};
        int operator[](std::size_t i) const { return data[i]; }
    };

    const rjk::duck<Subscriptable> x{Array{}};
    EXPECT_EQ(x[0UZ], 10);
    EXPECT_EQ(x[3UZ], 40);
}

TEST(SubscriptOperator, StdArray) {
    struct Subscriptable {
        int& operator[](std::size_t);
    };

    rjk::duck<Subscriptable> x{std::array{1,2,3,4}};
    x[0] = 99;
    EXPECT_EQ(x[0], 99);
}

TEST(SubscriptOperator, ConstAndNonConst) {
    struct Subscriptable {
        int& operator[](int);
        int operator[](int) const;
    };

    struct DualAccess {
        int data[3] = {5, 6, 7};
        int& operator[](int i)       { return data[i]; }
        int  operator[](int i) const { return data[i] * 2; }
    };

    rjk::duck<Subscriptable> x{DualAccess{}};
    const rjk::duck<Subscriptable> cx{DualAccess{}};

    EXPECT_EQ(cx[1], 12);  // const: returns doubled value
    x[1] = 99;
    EXPECT_EQ(rjk::get<DualAccess>(x).data[1], 99);
}

} // namespace rjk_test