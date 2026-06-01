// clang-format off
#include "duck.hpp"

#include <gtest/gtest.h>

namespace rjk_test {

// ============================================================================
// Basic overloading (regression)
// ============================================================================

TEST(DuckOverloading, Basic) {
    using MyDuck = rjk::duck<rjk::policy<
        rjk::has_fn<"compute", int(int)>,
        rjk::has_fn<"compute", double(double)>
    >>;

    struct Computer {
        int    compute(int)    { return 15; }
        double compute(double) { return 20.0; }
    };

    MyDuck x{Computer{}};

    EXPECT_EQ(x.compute(10),   15);
    EXPECT_EQ(x.compute(10.0), 20.0);
}

// ============================================================================
// Const overloading
// ============================================================================

// Two overloads distinguished only by const-ness.
TEST(DuckOverloading, ConstOverload) {
    using MyDuck = rjk::duck<rjk::policy<
        rjk::has_fn<"value", int()>,
        rjk::has_fn<"value", int() const>
    >>;

    struct Widget {
        int value()       { return 1; }
        int value() const { return 2; }
    };

    MyDuck x{Widget{}};
    const MyDuck cx{Widget{}};

    EXPECT_EQ(x.value(),  1);
    EXPECT_EQ(cx.value(), 2);
}

// Overload set with a mixed-const set: one overload is const, one isn't,
// plus a third plain overload with different args.
TEST(DuckOverloading, ConstAndParamOverload) {
    using MyDuck = rjk::duck<rjk::policy<
        rjk::has_fn<"act", int() const>,
        rjk::has_fn<"act", int(int)>
    >>;

    struct Source {
        int act() const    { return 42; }
        int act(int delta) { return 100 + delta; }
    };

    MyDuck x{Source{}};
    const MyDuck cx{Source{}};

    EXPECT_EQ(cx.act(),    42);
    EXPECT_EQ(x.act(7),   107);
}

// ============================================================================
// Ref-qualifier overloading
// ============================================================================

TEST(DuckOverloading, LvalueRvalueRefOverload) {
    using MyDuck = rjk::duck<rjk::policy<
        rjk::has_fn<"take", int() &>,
        rjk::has_fn<"take", int() &&>
    >>;

    struct Taker {
        int take() &  { return 1; }
        int take() && { return 2; }
    };

    MyDuck x{Taker{}};

    EXPECT_EQ(x.take(),             1);
    EXPECT_EQ(std::move(x).take(),  2);
}

TEST(DuckOverloading, ConstLvalueRvalueRefOverload) {
    using MyDuck = rjk::duck<rjk::policy<
        rjk::has_fn<"peek", int() const &>,
        rjk::has_fn<"peek", int() const &&>
    >>;

    struct Peeker {
        int peek() const &  { return 10; }
        int peek() const && { return 20; }
    };

    const MyDuck x{Peeker{}};

    EXPECT_EQ(x.peek(),             10);
    EXPECT_EQ(std::move(x).peek(),  20);
}

// ============================================================================
// 3+ overloads
// ============================================================================

TEST(DuckOverloading, ThreeParamOverloads) {
    using MyDuck = rjk::duck<rjk::policy<
        rjk::has_fn<"process", int(int)>,
        rjk::has_fn<"process", int(int, int)>,
        rjk::has_fn<"process", int(int, int, int)>
    >>;

    struct Processor {
        int process(int a)             { return a; }
        int process(int a, int b)      { return a + b; }
        int process(int a, int b, int c) { return a + b + c; }
    };

    MyDuck x{Processor{}};

    EXPECT_EQ(x.process(1),       1);
    EXPECT_EQ(x.process(1, 2),    3);
    EXPECT_EQ(x.process(1, 2, 3), 6);
}

TEST(DuckOverloading, FourOverloadsMixedTypes) {
    using MyDuck = rjk::duck<rjk::policy<
        rjk::has_fn<"format", std::string(int)>,
        rjk::has_fn<"format", std::string(std::string_view)>,
        rjk::has_fn<"format", std::string(const std::string&)>,
        rjk::has_fn<"format", std::string(bool)>
    >>;

    struct Formatter {
        std::string format(int x)               { return "int:" + std::to_string(x); }
        std::string format(std::string_view s)  { return std::string{"str_view:"} + s; }
        std::string format(const std::string& s){ return "str:" + s; }
        std::string format(bool b)              { return b ? "true" : "false"; }
    };

    MyDuck x{Formatter{}};

    EXPECT_EQ(x.format(1), "int:1");
    EXPECT_EQ(x.format(std::string_view{"3.14"}), "str_view:3.14");
    EXPECT_EQ(x.format(std::string{"hi"}), "str:hi");
    EXPECT_EQ(x.format(true), "true");
}

// ============================================================================
// Multiple distinct overload sets on the same duck
// ============================================================================

TEST(DuckOverloading, TwoOverloadedFunctions) {
    using MyDuck = rjk::duck<rjk::policy<
        rjk::has_fn<"encode", int(int)>,
        rjk::has_fn<"encode", int(bool)>,
        rjk::has_fn<"decode", std::string(int)>,
        rjk::has_fn<"decode", std::string(bool)>
    >>;

    struct Codec {
        int         encode(int x)    { return x * 2; }
        int         encode(bool x) { return x ? 6 : 0; }
        std::string decode(int x)    { return "i" + std::to_string(x); }
        std::string decode(bool x) { return "b" + std::to_string(x); }
    };

    MyDuck x{Codec{}};

    EXPECT_EQ(x.encode(5),    10);
    EXPECT_EQ(x.encode(true),  6);
    EXPECT_EQ(x.decode(7),    "i7");
    EXPECT_EQ(x.decode(false),  "b0");
}

// ============================================================================
// Operator overloading (op_plus, both sides)
// ============================================================================

TEST(DuckOperators, PlusLhsOverloads) {
    // Two lhs overloads of operator+ with different rhs types.
    using MyDuck = rjk::duck<rjk::policy<
        rjk::has_op<rjk::op_plus, int(const rjk::self&, int)>,
        rjk::has_op<rjk::op_plus, double(const rjk::self&, double)>
    >>;

    struct Adder {
        int    operator+(int rhs)    const { return 10 + rhs; }
        double operator+(double rhs) const { return 10.0 + rhs; }
    };

    MyDuck x{Adder{}};

    EXPECT_EQ(x + 5,    15);
    EXPECT_EQ(x + 2.5,  12.5);
}

struct Addend {
    int value = 5;
};

int    operator+(int lhs,    const Addend& rhs) { return lhs + rhs.value; }
double operator+(double lhs, const Addend& rhs) { return lhs + rhs.value; }

TEST(DuckOperators, PlusRhsOverloads) {
    // Two rhs overloads of operator+ with different lhs types.
    using MyDuck = rjk::duck<rjk::policy<
        rjk::has_op<rjk::op_plus, int(int, const rjk::self&)>,
        rjk::has_op<rjk::op_plus, double(double, const rjk::self&)>
    >>;

    MyDuck x{Addend{5}};

    EXPECT_EQ(3 + x,    8);
    EXPECT_EQ(1.5 + x,  6.5);
}

struct Symmetric {
    int value = 7;
    int operator+(int rhs) const { return value + rhs; }
};
int operator+(int lhs, const Symmetric& rhs) { return lhs + rhs.value; }

TEST(DuckOperators, PlusBothSides) {
    // One duck type that supports duck+int and int+duck simultaneously.
    using MyDuck = rjk::duck<rjk::policy<
        rjk::has_op<rjk::op_plus, int(const rjk::self&, int)>,
        rjk::has_op<rjk::op_plus, int(int, const rjk::self&)>
    >>;

    MyDuck x{Symmetric{7}};

    EXPECT_EQ(x + 3,  10);
    EXPECT_EQ(3 + x,  10);
}

// ============================================================================
// Mixed has_fn and has_op on the same duck
// ============================================================================

TEST(DuckOverloading, MixedFnAndOp) {
    using MyDuck = rjk::duck<rjk::policy<
        rjk::has_fn<"scale", int(int) const>,
        rjk::has_fn<"scale", double(double) const>,
        rjk::has_op<rjk::op_plus, int(const rjk::self&, int)>
    >>;

    struct Scaler {
        int    scale(int x)    const { return x * 2; }
        double scale(double x) const { return x * 2.0; }
        int    operator+(int rhs) const { return 100 + rhs; }
    };

    MyDuck x{Scaler{}};

    EXPECT_EQ(x.scale(3),    6);
    EXPECT_EQ(x.scale(1.5),  3.0);
    EXPECT_EQ(x + 1,         101);
}

// ============================================================================
// Swap underlying type after construction (regression for overload sets)
// ============================================================================

TEST(DuckOverloading, SwapUnderlyingType) {
    using MyDuck = rjk::duck<rjk::policy<
        rjk::has_fn<"compute", int(int)>,
        rjk::has_fn<"compute", double(double)>
    >>;

    struct DoubleComputer {
        int    compute(int x)    { return x * 2; }
        double compute(double x) { return x * 2.0; }
    };

    struct TripleComputer {
        int    compute(int x)    { return x * 3; }
        double compute(double x) { return x * 3.0; }
    };

    MyDuck x{DoubleComputer{}};
    EXPECT_EQ(x.compute(4),    8);
    EXPECT_EQ(x.compute(2.0),  4.0);

    x = TripleComputer{};
    EXPECT_EQ(x.compute(4),    12);
    EXPECT_EQ(x.compute(2.0),  6.0);
}

// ============================================================================
// Overloads that differ only in argument const-ness/ref-qualification
// ============================================================================

TEST(DuckOverloading, ConstRefArgOverloads) {
    using MyDuck = rjk::duck<rjk::policy<
        rjk::has_fn<"store", void(int&)>,
        rjk::has_fn<"store", void(const int&)>
    >>;

    struct Store {
        int last_mutable   = 0;
        int last_const     = 0;
        void store(int& x)       { last_mutable = x; x = 99; }
        void store(const int& x) { last_const   = x; }
    };

    MyDuck x{std::in_place_type<Store>};

    int mval = 5;
    x.store(mval);
    EXPECT_EQ(mval, 99); // mutable overload modified it

    const int cval = 7;
    x.store(cval);
    EXPECT_EQ(x.get<Store>().last_const, 7);
}

// ============================================================================
// Overload set with noexcept variants
// ============================================================================

TEST(DuckOverloading, NoexceptOverloads) {
    // noexcept is stripped during matching, so both overloads
    // should be found regardless of whether the member is noexcept.
    using MyDuck = rjk::duck<rjk::policy<
        rjk::has_fn<"run", int(int)>,
        rjk::has_fn<"run", int(double)>
    >>;

    struct Runner {
        int run(int x)    noexcept { return x + 1; }
        int run(double x) noexcept { return static_cast<int>(x) + 2; }
    };

    MyDuck x{Runner{}};

    EXPECT_EQ(x.run(10),   11);
    EXPECT_EQ(x.run(10.0), 12);
}

} // namespace rjk_test