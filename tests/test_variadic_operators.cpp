// clang-format off
#include "rjk/duck.hpp"

#include <gtest/gtest.h>
#include <vector>
#include <string>

namespace rjk_test {

// ============================================================================
// op_parentheses (operator())
// ============================================================================

TEST(CallOperator, NoArgs) {
    using CallDuck = rjk::duck<rjk::policy<
        rjk::has_op<rjk::op_parentheses, int()>
    >>;

    struct Counter {
        int value = 0;
        int operator()() { return ++value; }
    };

    CallDuck x{Counter{}};
    EXPECT_EQ(x(), 1);
    EXPECT_EQ(x(), 2);

    x = [data = 0] mutable {
        return data++;
    };
    EXPECT_EQ(x(), 0);
    EXPECT_EQ(x(), 1);
}

TEST(CallOperator, NoArgsConst) {
    using CallDuck = rjk::duck<rjk::policy<
        rjk::has_op<rjk::op_parentheses, int() const>
    >>;

    struct Getter {
        int value;
        int operator()() const { return value; }
    };

    const CallDuck x{Getter{42}};
    EXPECT_EQ(x(), 42);
}

TEST(CallOperator, SingleArg) {
    using CallDuck = rjk::duck<rjk::policy<
        rjk::has_op<rjk::op_parentheses, int(int) const>
    >>;

    struct Doubler {
        int operator()(int x) const { return x * 2; }
    };

    const CallDuck x{Doubler{}};
    EXPECT_EQ(x(5),  10);
    EXPECT_EQ(x(21), 42);
}

TEST(CallOperator, MultipleArgs) {
    using CallDuck = rjk::duck<rjk::policy<
        rjk::has_op<rjk::op_parentheses, int(int, int) const>
    >>;

    struct Adder {
        int operator()(int a, int b) const { return a + b; }
    };

    struct Multiplier {
        int operator()(int a, int b) const { return a * b; }
    };

    CallDuck x{Adder{}};
    EXPECT_EQ(x(3, 4), 7);

    x = Multiplier{};
    EXPECT_EQ(x(3, 4), 12);
}

TEST(CallOperator, ThreeArgs) {
    using CallDuck = rjk::duck<rjk::policy<
        rjk::has_op<rjk::op_parentheses, int(int, int, int) const>
    >>;

    struct Accumulator {
        int operator()(int a, int b, int c) const { return a + b + c; }
    };

    const CallDuck x{Accumulator{}};
    EXPECT_EQ(x(1, 2, 3), 6);
}

TEST(CallOperator, StringReturn) {
    using CallDuck = rjk::duck<rjk::policy<
        rjk::has_op<rjk::op_parentheses, std::string(const std::string&) const>
    >>;

    struct Greeter {
        std::string prefix;
        std::string operator()(const std::string& name) const {
            return prefix + name;
        }
    };

    const CallDuck x{Greeter{"Hello, "}};
    EXPECT_EQ(x("world"), "Hello, world");
}

TEST(CallOperator, ConstAndNonConstOverload) {
    using CallDuck = rjk::duck<rjk::policy<
        rjk::has_op<rjk::op_parentheses, int()>,
        rjk::has_op<rjk::op_parentheses, int() const>
    >>;

    struct Stateful {
        int calls = 0;
        int operator()()       { return ++calls; }
        int operator()() const { return 0; }
    };

    CallDuck x{Stateful{}};
    const CallDuck cx{Stateful{}};

    EXPECT_EQ(x(),  1);
    EXPECT_EQ(x(),  2);
    EXPECT_EQ(cx(), 0);
}

TEST(CallOperator, OverloadedOnArgs) {
    using CallDuck = rjk::duck<rjk::policy<
        rjk::has_op<rjk::op_parentheses, int(int) const>,
        rjk::has_op<rjk::op_parentheses, int(int, int) const>
    >>;

    struct Flexible {
        int operator()(int a)       const { return a; }
        int operator()(int a, int b) const { return a + b; }
    };

    const CallDuck x{Flexible{}};
    EXPECT_EQ(x(5),    5);
    EXPECT_EQ(x(3, 4), 7);
}

int multiply(int a, int b) {
    return a * b;
}

TEST(SubscriptOperator, CStyleArray) {
    struct [[=rjk::trait]] Subscriptable {
        int& operator[](int index);
    };

    int x[5] = {1,2,3,4,5};
    rjk::duck_view<Subscriptable> view{x};
    EXPECT_EQ(view[0], x[0]);
    EXPECT_EQ(view[3], x[3]);
}

TEST(SubscriptOperator, Basic) {
    using IndexDuck = rjk::duck<rjk::policy<
        rjk::has_op<rjk::op_square_brackets, int(std::size_t) const>
    >>;

    struct Array {
        int data[4] = {10, 20, 30, 40};
        int operator[](std::size_t i) const { return data[i]; }
    };

    const IndexDuck x{Array{}};
    EXPECT_EQ(x[0], 10);
    EXPECT_EQ(x[3], 40);
}

TEST(SubscriptOperator, NonConstReturnsRef) {
    using IndexDuck = rjk::duck<rjk::policy<
        rjk::has_op<rjk::op_square_brackets, int&(std::size_t)>
    >>;

    struct MutableArray {
        int data[4] = {1, 2, 3, 4};
        int& operator[](std::size_t i) { return data[i]; }
    };

    IndexDuck x{MutableArray{}};
    x[0] = 99;
    EXPECT_EQ(x.get<MutableArray>().data[0], 99);
}

TEST(SubscriptOperator, ConstAndNonConst) {
    using IndexDuck = rjk::duck<rjk::policy<
        rjk::has_op<rjk::op_square_brackets, int&(std::size_t)>,
        rjk::has_op<rjk::op_square_brackets, int(std::size_t) const>
    >>;

    struct DualAccess {
        int data[3] = {5, 6, 7};
        int& operator[](std::size_t i)       { return data[i]; }
        int  operator[](std::size_t i) const { return data[i] * 2; }
    };

    IndexDuck x{DualAccess{}};
    const IndexDuck cx{DualAccess{}};

    EXPECT_EQ(cx[1], 12);  // const: returns doubled value
    x[1] = 99;
    EXPECT_EQ(x.get<DualAccess>().data[1], 99);
}

TEST(SubscriptOperator, StringKey) {
    using MapDuck = rjk::duck<rjk::policy<
        rjk::has_op<rjk::op_square_brackets, int(const std::string&) const>
    >>;

    struct NamedValues {
        int operator[](const std::string& key) const {
            if (key == "a") return 1;
            if (key == "b") return 2;
            return -1;
        }
    };

    const MapDuck x{NamedValues{}};
    EXPECT_EQ(x["a"],       1);
    EXPECT_EQ(x["b"],       2);
    EXPECT_EQ(x["missing"], -1);
}

TEST(SubscriptOperator, SwapUnderlying) {
    using IndexDuck = rjk::duck<rjk::policy<
        rjk::has_op<rjk::op_square_brackets, int(std::size_t) const>
    >>;

    struct Zeros {
        int operator[](std::size_t) const { return 0; }
    };

    struct Ones {
        int operator[](std::size_t) const { return 1; }
    };

    IndexDuck x{Zeros{}};
    EXPECT_EQ(x[0], 0);

    x = Ones{};
    EXPECT_EQ(x[0], 1);
}

// ============================================================================
// Mixed call and subscript on same duck
// ============================================================================

TEST(CallAndSubscript, MixedOnSameDuck) {
    using Mixed = rjk::duck<rjk::policy<
        rjk::has_op<rjk::op_parentheses, int(int) const>,
        rjk::has_op<rjk::op_square_brackets,   int(int) const>
    >>;

    struct Lookup {
        int operator()(int x) const { return x * 2; }
        int operator[](int x) const { return x * 3; }
    };

    const Mixed x{Lookup{}};
    EXPECT_EQ(x(4),  8);
    EXPECT_EQ(x[4],  12);
}

} // namespace rjk_test