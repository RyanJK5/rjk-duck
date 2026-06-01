// clang-format off
#include "duck.hpp"

#include <compare>
#include <gtest/gtest.h>

namespace rjk_test {

// ============================================================================
// op_plus
// ============================================================================

TEST(BasicOperator, Plus) {
    using BasicPlus = rjk::duck<rjk::policy<
        rjk::has_op<rjk::op_plus, int(rjk::self, int)>,
        rjk::has_op<rjk::op_plus, int(int, rjk::self)>
    >>;

    BasicPlus x{10};

    EXPECT_EQ(x + 5,     15);
    EXPECT_EQ(5 + x + 5, 20);
}

TEST(BasicOperator, PlusOther) {
    using BasicPlus = rjk::duck<rjk::policy<
        rjk::has_op<rjk::op_plus, int(rjk::self&, const rjk::duck_t&)>
    >>;

    struct A {
        int operator+(const BasicPlus&) { return 5; }
    };

    struct B {
        int operator+(const BasicPlus&) { return 6; }
    };

    BasicPlus a{A{}};
    BasicPlus b{B{}};

    EXPECT_EQ(a + b, 5);
    EXPECT_EQ(b + a, 6);
}

TEST(BasicOperator, RefTest) {
    using RefPlus = rjk::duck<rjk::policy<
        rjk::has_op<rjk::op_plus, int(rjk::self&, rjk::duck_t&)>
    >>;

    struct A {
        int operator+(RefPlus&) const { return 10; }
        int operator+(RefPlus&)       { return 20; }
    };

    RefPlus x{A{}};
    RefPlus y{A{}};
    EXPECT_EQ(x + y, 20);
}

TEST(BasicOperator, ConstRefTest) {
    using ConstRefPlus = rjk::duck<rjk::policy<
        rjk::has_op<rjk::op_plus, int(const rjk::self&, const rjk::duck_t&)>
    >>;

    struct A {
        int operator+(const ConstRefPlus&)       { return 10; }
        int operator+(const ConstRefPlus&) const { return 20; }
    };

    ConstRefPlus x{A{}};
    ConstRefPlus y{A{}};
    EXPECT_EQ(x + y, 20);
}

TEST(BasicOperator, RValueTest) {
    using RValuePlus = rjk::duck<rjk::policy<
        rjk::has_op<rjk::op_plus, int(rjk::self&&, const rjk::duck_t&)>
    >>;

    struct A {
        int operator+(const RValuePlus&) &&      { return 10; }
        int operator+(const RValuePlus&) const & { return 20; }
    };

    RValuePlus x{A{}};
    RValuePlus y{A{}};
    EXPECT_EQ(RValuePlus{x} + std::move(y), 10);
}

TEST(BasicOperator, MultipleOverloads) {
    using Test = rjk::duck<rjk::policy<
        rjk::has_op<rjk::op_plus, int(rjk::self, const rjk::duck_t&)>
    >>;

    struct A {
        int operator+(const Test&) { return 5; }
        int operator+(int)         { return 10; }
    };

    Test x{A{}};
    EXPECT_EQ(x + Test{A{}}, 5);
}

// ============================================================================
// op_minus
// ============================================================================

struct Offset { int value; };
int operator-(int lhs, const Offset& rhs) { return lhs - rhs.value; }

struct Val {
    int value;
    int operator-(int rhs) const { return value - rhs; }
};
int operator-(int lhs, const Val& rhs) { return lhs - rhs.value; }

TEST(BasicOperator, MinusLhs) {
    using MinusDuck = rjk::duck<rjk::policy<
        rjk::has_op<rjk::op_minus, int(rjk::self, int)>
    >>;

    struct Counter {
        int value;
        int operator-(int rhs) const { return value - rhs; }
    };

    MinusDuck x{Counter{20}};
    EXPECT_EQ(x - 7, 13);
}

TEST(BasicOperator, MinusRhs) {
    using MinusDuck = rjk::duck<rjk::policy<
        rjk::has_op<rjk::op_minus, int(int, rjk::self)>
    >>;

    MinusDuck x{Offset{3}};
    EXPECT_EQ(10 - x, 7);
}

TEST(BasicOperator, MinusBothSides) {
    using MinusDuck = rjk::duck<rjk::policy<
        rjk::has_op<rjk::op_minus, int(rjk::self, int)>,
        rjk::has_op<rjk::op_minus, int(int, rjk::self)>
    >>;

    MinusDuck x{Val{10}};
    EXPECT_EQ(x - 3,  7);
    EXPECT_EQ(15 - x, 5);
}

// ============================================================================
// op_star (multiplication)
// ============================================================================

struct Factor {
    int value;
    int operator*(int rhs) const { return value * rhs; }
};
int operator*(int lhs, const Factor& rhs) { return lhs * rhs.value; }

TEST(BasicOperator, Star) {
    using MulDuck = rjk::duck<rjk::policy<
        rjk::has_op<rjk::op_star, int(rjk::self, int)>,
        rjk::has_op<rjk::op_star, int(int, rjk::self)>
    >>;

    MulDuck x{Factor{4}};
    EXPECT_EQ(x * 3,  12);
    EXPECT_EQ(3 * x,  12);
}

// ============================================================================
// op_slash, op_percent
// ============================================================================

TEST(BasicOperator, SlashAndPercent) {
    using DivDuck = rjk::duck<rjk::policy<
        rjk::has_op<rjk::op_slash,   int(rjk::self, int)>,
        rjk::has_op<rjk::op_percent, int(rjk::self, int)>
    >>;

    struct Num {
        int value;
        int operator/(int rhs)  const { return value / rhs; }
        int operator%(int rhs)  const { return value % rhs; }
    };

    DivDuck x{Num{17}};
    EXPECT_EQ(x / 5,  3);
    EXPECT_EQ(x % 5,  2);
}

// ============================================================================
// op_equals_equals, op_exclamation_equals
// ============================================================================

TEST(BasicOperator, EqualityOperators) {
    using EqDuck = rjk::duck<rjk::policy<
        rjk::has_op<rjk::op_equals_equals,      bool(rjk::self, int)>,
        rjk::has_op<rjk::op_exclamation_equals,  bool(rjk::self, int)>
    >>;

    struct Tag {
        int id;
        bool operator==(int rhs) const { return id == rhs; }
        bool operator!=(int rhs) const { return id != rhs; }
    };

    EqDuck x{Tag{42}};
    EXPECT_TRUE(x == 42);
    EXPECT_FALSE(x == 99);
    EXPECT_TRUE(x != 99);
    EXPECT_FALSE(x != 42);
}

// ============================================================================
// op_less, op_greater, op_less_equals, op_greater_equals
// ============================================================================

TEST(BasicOperator, RelationalOperators) {
    using CmpDuck = rjk::duck<rjk::policy<
        rjk::has_op<rjk::op_less,          bool(rjk::self, int)>,
        rjk::has_op<rjk::op_greater,        bool(rjk::self, int)>,
        rjk::has_op<rjk::op_less_equals,    bool(rjk::self, int)>,
        rjk::has_op<rjk::op_greater_equals, bool(rjk::self, int)>
    >>;

    struct Score {
        int value;
        bool operator<(int rhs)  const { return value < rhs; }
        bool operator>(int rhs)  const { return value > rhs; }
        bool operator<=(int rhs) const { return value <= rhs; }
        bool operator>=(int rhs) const { return value >= rhs; }
    };

    CmpDuck x{Score{5}};
    EXPECT_TRUE(x < 10);
    EXPECT_FALSE(x < 5);
    EXPECT_TRUE(x > 3);
    EXPECT_FALSE(x > 5);
    EXPECT_TRUE(x <= 5);
    EXPECT_TRUE(x >= 5);
    EXPECT_FALSE(x >= 6);
}

// ============================================================================
// op_spaceship
// ============================================================================

TEST(BasicOperator, Spaceship) {
    using SpaceshipDuck = rjk::duck<rjk::policy<
        rjk::has_op<rjk::op_spaceship, std::strong_ordering(rjk::self, int)>
    >>;

    struct Ranked {
        int value;
        std::strong_ordering operator<=>(int rhs) const { return value <=> rhs; }
    };

    SpaceshipDuck x{Ranked{5}};
    EXPECT_EQ(x <=> 5,  std::strong_ordering::equal);
    EXPECT_EQ(x <=> 3,  std::strong_ordering::greater);
    EXPECT_EQ(x <=> 10, std::strong_ordering::less);
}

// ============================================================================
// op_ampersand, op_pipe, op_caret (bitwise)
// ============================================================================

TEST(BasicOperator, BitwiseOperators) {
    using BitDuck = rjk::duck<rjk::policy<
        rjk::has_op<rjk::op_ampersand, int(rjk::self, int)>,
        rjk::has_op<rjk::op_pipe,      int(rjk::self, int)>,
        rjk::has_op<rjk::op_caret,     int(rjk::self, int)>
    >>;

    struct Bits {
        int value;
        int operator&(int rhs) const { return value & rhs; }
        int operator|(int rhs) const { return value | rhs; }
        int operator^(int rhs) const { return value ^ rhs; }
    };

    BitDuck x{Bits{0b1010}};
    EXPECT_EQ(x & 0b1100, 0b1000);
    EXPECT_EQ(x | 0b0101, 0b1111);
    EXPECT_EQ(x ^ 0b1111, 0b0101);
}

// ============================================================================
// op_less_less, op_greater_greater (shift)
// ============================================================================

TEST(BasicOperator, ShiftOperators) {
    using ShiftDuck = rjk::duck<rjk::policy<
        rjk::has_op<rjk::op_less_less,       int(rjk::self, int)>,
        rjk::has_op<rjk::op_greater_greater,  int(rjk::self, int)>
    >>;

    struct Shiftable {
        int value;
        int operator<<(int n) const { return value << n; }
        int operator>>(int n) const { return value >> n; }
    };

    ShiftDuck x{Shiftable{8}};
    EXPECT_EQ(x << 2, 32);
    EXPECT_EQ(x >> 1, 4);
}

// ============================================================================
// op_ampersand_ampersand, op_pipe_pipe (logical)
// ============================================================================

TEST(BasicOperator, LogicalOperators) {
    using LogicDuck = rjk::duck<rjk::policy<
        rjk::has_op<rjk::op_ampersand_ampersand, bool(rjk::self, bool)>,
        rjk::has_op<rjk::op_pipe_pipe,            bool(rjk::self, bool)>
    >>;

    struct Flag {
        bool value;
        bool operator&&(bool rhs) const { return value && rhs; }
        bool operator||(bool rhs) const { return value || rhs; }
    };

    LogicDuck t{Flag{true}};
    LogicDuck f{Flag{false}};

    EXPECT_TRUE(t && true);
    EXPECT_FALSE(t && false);
    EXPECT_FALSE(f && true);
    EXPECT_TRUE(f || true);
    EXPECT_FALSE(f || false);
}

// ============================================================================
// Compound assignment: op_plus_equals, op_minus_equals, op_star_equals
// ============================================================================

TEST(BasicOperator, CompoundAssignment) {
    using CompoundDuck = rjk::duck<rjk::policy<
        rjk::has_op<rjk::op_plus_equals,  void(rjk::self&, int)>,
        rjk::has_op<rjk::op_minus_equals, void(rjk::self&, int)>,
        rjk::has_op<rjk::op_star_equals,  void(rjk::self&, int)>,
        rjk::has_fn<"get_value", int() const>
    >>;

    struct Accum {
        int value;
        void operator+=(int rhs) { value += rhs; }
        void operator-=(int rhs) { value -= rhs; }
        void operator*=(int rhs) { value *= rhs; }
        int get_value() const { return value; }
    };

    CompoundDuck x{Accum{10}};
    x += 5;
    EXPECT_EQ(x.get_value(), 15);
    x -= 3;
    EXPECT_EQ(x.get_value(), 12);
    x *= 2;
    EXPECT_EQ(x.get_value(), 24);
}

// ============================================================================
// Unary op_minus, op_exclamation, op_tilde
// ============================================================================

TEST(BasicOperator, UnaryMinus) {
    using NegDuck = rjk::duck<rjk::policy<
        rjk::has_op<rjk::op_minus, int(rjk::self)>
    >>;

    struct Signed {
        int value;
        int operator-() const { return -value; }
    };

    NegDuck x{Signed{7}};
    EXPECT_EQ(-x, -7);
}

TEST(BasicOperator, UnaryNot) {
    using NotDuck = rjk::duck<rjk::policy<
        rjk::has_op<rjk::op_exclamation, bool(rjk::self)>
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

TEST(BasicOperator, UnaryBitwiseNot) {
    using TildeDuck = rjk::duck<rjk::policy<
        rjk::has_op<rjk::op_tilde, int(rjk::self)>
    >>;

    struct Maskable {
        int value;
        int operator~() const { return ~value; }
    };

    TildeDuck x{Maskable{0b1010}};
    EXPECT_EQ(~x, ~0b1010);
}

TEST(BasicOperator, Comma) {
    using CommaDuck = rjk::duck<rjk::policy<
        rjk::has_op<rjk::op_comma, int(rjk::self, int)>
    >>;

    struct Sequenced {
        int value;
        int operator,(int rhs) const { return value + rhs; }
    };

    CommaDuck x{Sequenced{10}};
    EXPECT_EQ((x, 5), 15);
}

// ============================================================================
// Mixed unary and binary on the same duck
// ============================================================================

TEST(BasicOperator, MixedUnaryAndBinary) {
    using MixedDuck = rjk::duck<rjk::policy<
        rjk::has_op<rjk::op_minus,           int(rjk::self)>,
        rjk::has_op<rjk::op_minus,           int(rjk::self, int)>,
        rjk::has_op<rjk::op_exclamation,     bool(rjk::self)>
    >>;

    struct Val {
        int value;
        int  operator-()      const { return -value; }
        int  operator-(int n) const { return value - n; }
        bool operator!()      const { return value == 0; }
    };

    MixedDuck x{Val{5}};
    EXPECT_EQ(-x,    -5);
    EXPECT_EQ(x - 2,  3);
    EXPECT_FALSE(!x);

    MixedDuck zero{Val{0}};
    EXPECT_TRUE(!zero);
}

// ============================================================================
// Prefix/postfix increment and decrement
// ============================================================================

TEST(BasicOperator, PrefixIncrement) {
    using PreIncDuck = rjk::duck<rjk::policy<
        rjk::has_op<rjk::op_plus_plus, int(rjk::self&)>
    >>;

    struct Counter {
        int value;
        int operator++() { return ++value; }
    };

    PreIncDuck x{Counter{5}};
    EXPECT_EQ(++x, 6);
    EXPECT_EQ(++x, 7);
}

TEST(BasicOperator, PostfixIncrement) {
    using PostIncDuck = rjk::duck<rjk::policy<
        rjk::has_op<rjk::op_plus_plus, int(rjk::self&, int)>
    >>;

    struct Counter {
        int value;
        int operator++(int) { return value++; }
    };

    PostIncDuck x{Counter{5}};
    EXPECT_EQ(x++, 5);
    EXPECT_EQ(x++, 6);
}

TEST(BasicOperator, PrefixDecrement) {
    using PreDecDuck = rjk::duck<rjk::policy<
        rjk::has_op<rjk::op_minus_minus, int(rjk::self&)>
    >>;

    struct Counter {
        int value;
        int operator--() { return --value; }
    };

    PreDecDuck x{Counter{5}};
    EXPECT_EQ(--x, 4);
    EXPECT_EQ(--x, 3);
}

TEST(BasicOperator, PostfixDecrement) {
    using PostDecDuck = rjk::duck<rjk::policy<
        rjk::has_op<rjk::op_minus_minus, int(rjk::self&, int)>
    >>;

    struct Counter {
        int value;
        int operator--(int) { return value--; }
    };

    PostDecDuck x{Counter{5}};
    EXPECT_EQ(x--, 5);
    EXPECT_EQ(x--, 4);
}

TEST(BasicOperator, PreAndPostIncrement) {
    // Both prefix and postfix on the same duck.
    using IncDuck = rjk::duck<rjk::policy<
        rjk::has_op<rjk::op_plus_plus, int(rjk::self&)>,
        rjk::has_op<rjk::op_plus_plus, int(rjk::self&, int)>
    >>;

    struct Counter {
        int value;
        int operator++()    { return ++value; }
        int operator++(int) { return value++; }
    };

    IncDuck x{Counter{0}};
    EXPECT_EQ(x++, 0);  // post: returns old, increments
    EXPECT_EQ(++x, 2);  // pre:  increments, returns new
    EXPECT_EQ(x++, 2);  // post: returns old again
}

TEST(BasicOperator, Arrow) {
    struct Foo {
        int value = 10;
        int doSmth() { return value; }
    };
    using Container = rjk::duck<rjk::policy<
        rjk::has_op<rjk::op_star, Foo() const>,
        rjk::has_op<rjk::op_arrow, Foo*()>
    >>;

    struct MyContainer {
        Foo f{};

        Foo operator*() const { return f; }
        Foo* operator->() { return &f;}
    };

    Container c{MyContainer{}};

    EXPECT_EQ((*c).doSmth(), 10);
    EXPECT_EQ(c->doSmth(), 10);

    c->value = 5;
    EXPECT_EQ(c->doSmth(), 5);
}

TEST(BasicOperator, ArrowStar) {
    struct Foo {
        int value = 10;
        int doSmth() { return value + 5; }
    };

    struct MemberFuncInvoker {
        Foo* instance;
        int (Foo::*func)();
        int operator()() const { return (instance->*func)(); }
    };

    // TODO: Change to raw int& when GCC fixes bug
    using Container = rjk::duck<rjk::policy<
        rjk::has_op<rjk::op_arrow, Foo*()>,
        rjk::has_op<rjk::op_arrow_star, std::reference_wrapper<int>(int Foo::*)>,
        rjk::has_op<rjk::op_arrow_star, MemberFuncInvoker(int (Foo::*)())>
    >>;

    struct MyContainer {
        Foo f{};

        Foo* operator->() { return &f; }

        std::reference_wrapper<int> operator->*(int Foo::* data_ptr) {
            return f.*data_ptr;
        }

        MemberFuncInvoker operator->*(int (Foo::* func_ptr)()) {
            return MemberFuncInvoker{&f, func_ptr};
        }
    };

    Container c{MyContainer{}};

    int Foo::* data_ptr = &Foo::value;
    int (Foo::* func_ptr)() = &Foo::doSmth;

    EXPECT_EQ(c->*data_ptr, 10);
    (c->*data_ptr).get() = 42;
    EXPECT_EQ(c->value, 42);

    EXPECT_EQ((c->*func_ptr)(), 47); // 42 + 5
}

} // namespace rjk_test