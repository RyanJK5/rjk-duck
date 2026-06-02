// clang-format off
#include "duck.hpp"

#include <compare>
#include <gtest/gtest.h>

namespace rjk_test {

// ============================================================================
// op_plus
// ============================================================================

TEST(BasicOperator, Plus) {
    struct [[=rjk::trait]] BasicPlus {
        [[=rjk::both_sides]]
        int operator+(int);
    };

    rjk::duck<BasicPlus> x{10};

    EXPECT_EQ(x + 5,     15);
    EXPECT_EQ(5 + x + 5, 20);
}

TEST(BasicOperator, PlusOther) {
    struct [[=rjk::trait]] PlusOtherPolicy {
        int operator+(const rjk::duck_t&);
    };
    using BasicPlus = rjk::duck<PlusOtherPolicy>;

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
    struct [[=rjk::trait]] RefPlusPolicy {
        int operator+(rjk::duck_t&);
    };
    using RefPlus = rjk::duck<RefPlusPolicy>;

    struct A {
        int operator+(RefPlus&) const { return 10; }
        int operator+(RefPlus&)       { return 20; }
    };

    RefPlus x{A{}};
    RefPlus y{A{}};
    EXPECT_EQ(x + y, 20);
}

TEST(BasicOperator, ConstRefTest) {
    struct [[=rjk::trait]] ConstRefPlusPolicy {
        int operator+(const rjk::duck_t&) const;
    };
    using ConstRefPlus = rjk::duck<ConstRefPlusPolicy>;

    struct A {
        int operator+(const ConstRefPlus&)       { return 10; }
        int operator+(const ConstRefPlus&) const { return 20; }
    };

    ConstRefPlus x{A{}};
    ConstRefPlus y{A{}};
    EXPECT_EQ(x + y, 20);
}

TEST(BasicOperator, RValueTest) {
    struct [[=rjk::trait]] RValuePlusPolicy {
        int operator+(const rjk::duck_t&) &&;
    };
    using RValuePlus = rjk::duck<RValuePlusPolicy>;

    struct A {
        int operator+(const RValuePlus&) &&      { return 10; }
        int operator+(const RValuePlus&) const & { return 20; }
    };

    RValuePlus x{A{}};
    RValuePlus y{A{}};
    EXPECT_EQ(RValuePlus{x} + std::move(y), 10);
}

TEST(BasicOperator, MultipleOverloads) {
    struct [[=rjk::trait]] MultiOverloadPolicy {
        int operator+(const rjk::duck_t&);
        int operator+(int);
    };
    using Test = rjk::duck<MultiOverloadPolicy>;

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

struct [[=rjk::trait]] MinusLhsPolicy {
    int operator-(int) const;
};

TEST(BasicOperator, MinusLhs) {
    struct Counter {
        int value;
        int operator-(int rhs) const { return value - rhs; }
    };

    rjk::duck<MinusLhsPolicy> x{Counter{20}};
    EXPECT_EQ(x - 7, 13);
}

struct [[=rjk::trait]] MinusRhsPolicy {
    [[=rjk::rhs_op]]
    int operator-(int);
};

TEST(BasicOperator, MinusRhs) {
    rjk::duck<MinusRhsPolicy> x{Offset{3}};
    EXPECT_EQ(10 - x, 7);
}

struct [[=rjk::trait]] MinusBothPolicy {
    [[=rjk::both_sides]]
    int operator-(int);
};

TEST(BasicOperator, MinusBothSides) {
    rjk::duck<MinusBothPolicy> x{Val{10}};
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

struct [[=rjk::trait]] StarPolicy {
    [[=rjk::both_sides]]
    int operator*(int);
};

TEST(BasicOperator, Star) {
    rjk::duck<StarPolicy> x{Factor{4}};
    EXPECT_EQ(x * 3, 12);
    EXPECT_EQ(3 * x, 12);
}

// ============================================================================
// op_slash, op_percent
// ============================================================================

struct [[=rjk::trait]] DivPolicy {
    int operator/(int) const;
    int operator%(int) const;
};

TEST(BasicOperator, SlashAndPercent) {
    struct Num {
        int value;
        int operator/(int rhs) const { return value / rhs; }
        int operator%(int rhs) const { return value % rhs; }
    };

    rjk::duck<DivPolicy> x{Num{17}};
    EXPECT_EQ(x / 5, 3);
    EXPECT_EQ(x % 5, 2);
}

// ============================================================================
// op_equals_equals, op_exclamation_equals
// ============================================================================

struct [[=rjk::trait]] EqPolicy {
    bool operator==(int) const;
    bool operator!=(int) const;
};

TEST(BasicOperator, EqualityOperators) {
    struct Tag {
        int id;
        bool operator==(int rhs) const { return id == rhs; }
        bool operator!=(int rhs) const { return id != rhs; }
    };

    rjk::duck<EqPolicy> x{Tag{42}};
    EXPECT_TRUE(x == 42);
    EXPECT_FALSE(x == 99);
    EXPECT_TRUE(x != 99);
    EXPECT_FALSE(x != 42);
}

// ============================================================================
// op_less, op_greater, op_less_equals, op_greater_equals
// ============================================================================

struct [[=rjk::trait]] CmpPolicy {
    bool operator<(int)  const;
    bool operator>(int)  const;
    bool operator<=(int) const;
    bool operator>=(int) const;
};

TEST(BasicOperator, RelationalOperators) {
    struct Score {
        int value;
        bool operator<(int rhs)  const { return value < rhs; }
        bool operator>(int rhs)  const { return value > rhs; }
        bool operator<=(int rhs) const { return value <= rhs; }
        bool operator>=(int rhs) const { return value >= rhs; }
    };

    rjk::duck<CmpPolicy> x{Score{5}};
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

struct [[=rjk::trait]] SpaceshipPolicy {
    std::strong_ordering operator<=>(int) const;
};

TEST(BasicOperator, Spaceship) {
    struct Ranked {
        int value;
        std::strong_ordering operator<=>(int rhs) const { return value <=> rhs; }
    };

    rjk::duck<SpaceshipPolicy> x{Ranked{5}};
    EXPECT_EQ(x <=> 5,  std::strong_ordering::equal);
    EXPECT_EQ(x <=> 3,  std::strong_ordering::greater);
    EXPECT_EQ(x <=> 10, std::strong_ordering::less);
}

// ============================================================================
// op_ampersand, op_pipe, op_caret (bitwise)
// ============================================================================

struct [[=rjk::trait]] BitPolicy {
    int operator&(int) const;
    int operator|(int) const;
    int operator^(int) const;
};

TEST(BasicOperator, BitwiseOperators) {
    struct Bits {
        int value;
        int operator&(int rhs) const { return value & rhs; }
        int operator|(int rhs) const { return value | rhs; }
        int operator^(int rhs) const { return value ^ rhs; }
    };

    rjk::duck<BitPolicy> x{Bits{0b1010}};
    EXPECT_EQ(x & 0b1100, 0b1000);
    EXPECT_EQ(x | 0b0101, 0b1111);
    EXPECT_EQ(x ^ 0b1111, 0b0101);
}

// ============================================================================
// op_less_less, op_greater_greater (shift)
// ============================================================================

struct [[=rjk::trait]] ShiftPolicy {
    int operator<<(int) const;
    int operator>>(int) const;
};

TEST(BasicOperator, ShiftOperators) {
    struct Shiftable {
        int value;
        int operator<<(int n) const { return value << n; }
        int operator>>(int n) const { return value >> n; }
    };

    rjk::duck<ShiftPolicy> x{Shiftable{8}};
    EXPECT_EQ(x << 2, 32);
    EXPECT_EQ(x >> 1, 4);
}

// ============================================================================
// op_ampersand_ampersand, op_pipe_pipe (logical)
// ============================================================================

struct [[=rjk::trait]] LogicPolicy {
    bool operator&&(bool) const;
    bool operator||(bool) const;
};

TEST(BasicOperator, LogicalOperators) {
    struct Flag {
        bool value;
        bool operator&&(bool rhs) const { return value && rhs; }
        bool operator||(bool rhs) const { return value || rhs; }
    };

    rjk::duck<LogicPolicy> t{Flag{true}};
    rjk::duck<LogicPolicy> f{Flag{false}};

    EXPECT_TRUE(t && true);
    EXPECT_FALSE(t && false);
    EXPECT_FALSE(f && true);
    EXPECT_TRUE(f || true);
    EXPECT_FALSE(f || false);
}

// ============================================================================
// Compound assignment: op_plus_equals, op_minus_equals, op_star_equals
// ============================================================================

struct [[=rjk::trait]] CompoundPolicy {
    void operator+=(int);
    void operator-=(int);
    void operator*=(int);
    int get_value() const;
};

TEST(BasicOperator, CompoundAssignment) {
    struct Accum {
        int value;
        void operator+=(int rhs) { value += rhs; }
        void operator-=(int rhs) { value -= rhs; }
        void operator*=(int rhs) { value *= rhs; }
        int get_value() const { return value; }
    };

    rjk::duck<CompoundPolicy> x{Accum{10}};
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

struct [[=rjk::trait]] NegPolicy {
    int operator-() const;
};

TEST(BasicOperator, UnaryMinus) {
    struct Signed {
        int value;
        int operator-() const { return -value; }
    };

    rjk::duck<NegPolicy> x{Signed{7}};
    EXPECT_EQ(-x, -7);
}

struct [[=rjk::trait]] NotPolicy {
    bool operator!() const;
};

TEST(BasicOperator, UnaryNot) {
    struct Truthy {
        bool value;
        bool operator!() const { return !value; }
    };

    rjk::duck<NotPolicy> t{Truthy{true}};
    rjk::duck<NotPolicy> f{Truthy{false}};
    EXPECT_FALSE(!t);
    EXPECT_TRUE(!f);
}

struct [[=rjk::trait]] TildePolicy {
    int operator~() const;
};

TEST(BasicOperator, UnaryBitwiseNot) {
    struct Maskable {
        int value;
        int operator~() const { return ~value; }
    };

    rjk::duck<TildePolicy> x{Maskable{0b1010}};
    EXPECT_EQ(~x, ~0b1010);
}

struct [[=rjk::trait]] CommaPolicy {
    int operator,(int) const;
};

TEST(BasicOperator, Comma) {
    struct Sequenced {
        int value;
        int operator,(int rhs) const { return value + rhs; }
    };

    rjk::duck<CommaPolicy> x{Sequenced{10}};
    EXPECT_EQ((x, 5), 15);
}

// ============================================================================
// Mixed unary and binary on the same duck
// ============================================================================

struct [[=rjk::trait]] MixedPolicy {
    int  operator-()      const;
    int  operator-(int)   const;
    bool operator!()      const;
};

TEST(BasicOperator, MixedUnaryAndBinary) {
    struct Val {
        int value;
        int  operator-()      const { return -value; }
        int  operator-(int n) const { return value - n; }
        bool operator!()      const { return value == 0; }
    };

    rjk::duck<MixedPolicy> x{Val{5}};
    EXPECT_EQ(-x,    -5);
    EXPECT_EQ(x - 2,  3);
    EXPECT_FALSE(!x);

    rjk::duck<MixedPolicy> zero{Val{0}};
    EXPECT_TRUE(!zero);
}

// ============================================================================
// Prefix/postfix increment and decrement
// ============================================================================

struct [[=rjk::trait]] PreIncPolicy  { int operator++(); };
struct [[=rjk::trait]] PostIncPolicy { int operator++(int); };
struct [[=rjk::trait]] PreDecPolicy  { int operator--(); };
struct [[=rjk::trait]] PostDecPolicy { int operator--(int); };

TEST(BasicOperator, PrefixIncrement) {
    struct Counter {
        int value;
        int operator++() { return ++value; }
    };

    rjk::duck<PreIncPolicy> x{Counter{5}};
    EXPECT_EQ(++x, 6);
    EXPECT_EQ(++x, 7);
}

TEST(BasicOperator, PostfixIncrement) {
    struct Counter {
        int value;
        int operator++(int) { return value++; }
    };

    rjk::duck<PostIncPolicy> x{Counter{5}};
    EXPECT_EQ(x++, 5);
    EXPECT_EQ(x++, 6);
}

TEST(BasicOperator, PrefixDecrement) {
    struct Counter {
        int value;
        int operator--() { return --value; }
    };

    rjk::duck<PreDecPolicy> x{Counter{5}};
    EXPECT_EQ(--x, 4);
    EXPECT_EQ(--x, 3);
}

TEST(BasicOperator, PostfixDecrement) {
    struct Counter {
        int value;
        int operator--(int) { return value--; }
    };

    rjk::duck<PostDecPolicy> x{Counter{5}};
    EXPECT_EQ(x--, 5);
    EXPECT_EQ(x--, 4);
}


TEST(BasicOperator, PreAndPostIncrement) {
    struct [[=rjk::trait]] IncPolicy {
        int operator++();
        int operator++(int);
    };

    struct Counter {
        int value;
        int operator++()    { return ++value; }
        int operator++(int) { return value++; }
    };

    rjk::duck<IncPolicy> x{Counter{0}};
    EXPECT_EQ(x++, 0);  // post: returns old, increments
    EXPECT_EQ(++x, 2);  // pre:  increments, returns new
    EXPECT_EQ(x++, 2);  // post: returns old again
}

// ============================================================================
// Arrow
// ============================================================================

TEST(BasicOperator, Arrow) {
    struct ArrowFoo {
        int value = 10;
        int doSmth() { return value; }
    };

    struct [[=rjk::trait]] ArrowPolicy {
        ArrowFoo  operator*() const;
        ArrowFoo* operator->();
    };

    struct MyContainer {
        ArrowFoo f{};
        ArrowFoo  operator*() const { return f; }
        ArrowFoo* operator->()      { return &f; }
    };

    rjk::duck<ArrowPolicy> c{MyContainer{}};

    EXPECT_EQ((*c).doSmth(), 10);
    EXPECT_EQ(c->doSmth(),   10);

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
    struct [[=rjk::trait]] Container {
        Foo* operator->();
        std::reference_wrapper<int> operator->*(int Foo::*);
        MemberFuncInvoker operator->*(int (Foo::*)());
    };

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

    rjk::duck<Container> c{MyContainer{}};

    int Foo::* data_ptr = &Foo::value;
    int (Foo::* func_ptr)() = &Foo::doSmth;

    EXPECT_EQ(c->*data_ptr, 10);
    (c->*data_ptr).get() = 42;
    EXPECT_EQ(c->value, 42);

    EXPECT_EQ((c->*func_ptr)(), 47); // 42 + 5
}

} // namespace rjk_test