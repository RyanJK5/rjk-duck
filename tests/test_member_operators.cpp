#include "duck.hpp"

#include <gtest/gtest.h>

namespace rjk_test {

// ============================================================================
// Member function syntax
// ============================================================================

TEST(MemberFunctionSyntax, UnaryMinus) {
    using NegDuck = rjk::duck<rjk::policy<
        rjk::has_op<rjk::op_minus, int()>
    >>;

    struct Signed {
        int value;
        int operator-() const { return -value; }
    };

    NegDuck x{Signed{7}};
    EXPECT_EQ(-x, -7);
}

TEST(MemberFunctionSyntax, UnaryConst) {
    using NotDuck = rjk::duck<rjk::policy<
        rjk::has_op<rjk::op_exclamation, bool() const>
    >>;

    struct Truthy {
        bool value;
        bool operator!() const { return !value; }
    };

    NotDuck t{Truthy{true}};
    NotDuck f{Truthy{false}};
    EXPECT_FALSE(!t);
    EXPECT_TRUE(!f);
}

TEST(MemberFunctionSyntax, BinaryLhsNonConst) {
    using PlusDuck = rjk::duck<rjk::policy<
        rjk::has_op<rjk::op_plus, int(int)>
    >>;

    struct Adder {
        int value;
        int operator+(int rhs) { return value + rhs; }
    };

    PlusDuck x{Adder{10}};
    EXPECT_EQ(x + 5, 15);
}

TEST(MemberFunctionSyntax, BinaryLhsConst) {
    using PlusDuck = rjk::duck<rjk::policy<
        rjk::has_op<rjk::op_plus, int(int) const>
    >>;

    struct Adder {
        int value;
        int operator+(int rhs) const { return value + rhs; }
    };

    const PlusDuck x{Adder{10}};
    EXPECT_EQ(x + 5, 15);
}

TEST(MemberFunctionSyntax, BinaryLhsRvalueRef) {
    using PlusDuck = rjk::duck<rjk::policy<
        rjk::has_op<rjk::op_plus, int(int) &&>
    >>;

    struct Adder {
        int value;
        int operator+(int rhs) && { return value + rhs; }
    };

    PlusDuck x{Adder{10}};
    EXPECT_EQ(std::move(x) + 5, 15);
}

TEST(MemberFunctionSyntax, ConstAndNonConstOverload) {
    // Same operator, distinguished only by const-ness of self.
    using PlusDuck = rjk::duck<rjk::policy<
        rjk::has_op<rjk::op_plus, int(int)>,
        rjk::has_op<rjk::op_plus, int(int) const>
    >>;

    struct Adder {
        int operator+(int rhs)       { return 1 + rhs; }
        int operator+(int rhs) const { return 2 + rhs; }
    };

    PlusDuck x{Adder{}};
    const PlusDuck cx{Adder{}};
    EXPECT_EQ(x + 10,  11);
    EXPECT_EQ(cx + 10, 12);
}

struct Adder {
    int value;
    int operator+(int rhs) const { return value + rhs; }
};
int operator+(int lhs, const Adder& rhs) { return lhs + rhs.value; }


TEST(MemberFunctionSyntax, MixedMemberAndExplicitSelf) {
    // Member style for lhs, explicit self for rhs on the same duck.
    using PlusDuck = rjk::duck<rjk::policy<
        rjk::has_op<rjk::op_plus, int(int) const>,
        rjk::has_op<rjk::op_plus, int(int, rjk::self)>
    >>;

    PlusDuck x{Adder{10}};
    EXPECT_EQ(x + 5,  15);
    EXPECT_EQ(5 + x,  15);
}

TEST(MemberFunctionSyntax, MultipleOperatorsMemberStyle) {
    using MathDuck = rjk::duck<rjk::policy<
        rjk::has_op<rjk::op_plus,    int(int) const>,
        rjk::has_op<rjk::op_minus,   int(int) const>,
        rjk::has_op<rjk::op_star,    int(int) const>,
        rjk::has_op<rjk::op_exclamation, bool() const>
    >>;

    struct Math {
        int value;
        int  operator+(int rhs)  const { return value + rhs; }
        int  operator-(int rhs)  const { return value - rhs; }
        int  operator*(int rhs)  const { return value * rhs; }
        bool operator!()         const { return value == 0; }
    };

    const MathDuck x{Math{5}};
    EXPECT_EQ(x + 3,   8);
    EXPECT_EQ(x - 3,   2);
    EXPECT_EQ(x * 3,   15);
    EXPECT_FALSE(!x);

    const MathDuck zero{Math{0}};
    EXPECT_TRUE(!zero);
}
}
