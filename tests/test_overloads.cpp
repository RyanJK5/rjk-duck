// clang-format off
#include "rjk/duck.hpp"

#include <gtest/gtest.h>

namespace rjk_test {

// ============================================================================
// Basic overloading (regression)
// ============================================================================

struct ComputePolicy {
    auto compute(int)    -> int;
    auto compute(double) -> double;
};

TEST(DuckOverloading, Basic) {
    using MyDuck = rjk::duck<ComputePolicy>;

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
struct ValuePolicy {
    auto value() -> int;
    auto value() const -> int;
};

TEST(DuckOverloading, ConstOverload) {
    using MyDuck = rjk::duck<ValuePolicy>;

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
struct ActPolicy {
    auto act() const -> int;
    auto act(int)    -> int;
};

TEST(DuckOverloading, ConstAndParamOverload) {
    using MyDuck = rjk::duck<ActPolicy>;

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

struct TakePolicy {
    auto take() &  -> int;
    auto take() && -> int;
};

TEST(DuckOverloading, LvalueRvalueRefOverload) {
    using MyDuck = rjk::duck<TakePolicy>;

    struct Taker {
        int take() &  { return 1; }
        int take() && { return 2; }
    };

    MyDuck x{Taker{}};

    EXPECT_EQ(x.take(),             1);
    EXPECT_EQ(std::move(x).take(),  2);
}

struct PeekPolicy {
    auto peek() const &  -> int;
    auto peek() const && -> int;
};

TEST(DuckOverloading, ConstLvalueRvalueRefOverload) {
    using MyDuck = rjk::duck<PeekPolicy>;

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

struct ProcessPolicy {
    auto process(int)           -> int;
    auto process(int, int)      -> int;
    auto process(int, int, int) -> int;
};

TEST(DuckOverloading, ThreeParamOverloads) {
    using MyDuck = rjk::duck<ProcessPolicy>;

    struct Processor {
        int process(int a)               { return a; }
        int process(int a, int b)        { return a + b; }
        int process(int a, int b, int c) { return a + b + c; }
    };

    MyDuck x{Processor{}};

    EXPECT_EQ(x.process(1),       1);
    EXPECT_EQ(x.process(1, 2),    3);
    EXPECT_EQ(x.process(1, 2, 3), 6);
}

struct FormatPolicy {
    auto format(int)                -> std::string;
    auto format(std::string_view)   -> std::string;
    auto format(const std::string&) -> std::string;
    auto format(bool)               -> std::string;
};

TEST(DuckOverloading, FourOverloadsMixedTypes) {
    using MyDuck = rjk::duck<FormatPolicy>;

    struct Formatter {
        std::string format(int x)                { return "int:" + std::to_string(x); }
        std::string format(std::string_view s)   { return std::string{"str_view:"} + s; }
        std::string format(const std::string& s) { return "str:" + s; }
        std::string format(bool b)               { return b ? "true" : "false"; }
    };

    MyDuck x{Formatter{}};

    EXPECT_EQ(x.format(1),                        "int:1");
    EXPECT_EQ(x.format(std::string_view{"3.14"}), "str_view:3.14");
    EXPECT_EQ(x.format(std::string{"hi"}),        "str:hi");
    EXPECT_EQ(x.format(true),                     "true");
}

// ============================================================================
// Multiple distinct overload sets on the same duck
// ============================================================================

struct CodecPolicy {
    auto encode(int)  -> int;
    auto encode(bool) -> int;
    auto decode(int)  -> std::string;
    auto decode(bool) -> std::string;
};

TEST(DuckOverloading, TwoOverloadedFunctions) {
    using MyDuck = rjk::duck<CodecPolicy>;

    struct Codec {
        int         encode(int x)  { return x * 2; }
        int         encode(bool x) { return x ? 6 : 0; }
        std::string decode(int x)  { return "i" + std::to_string(x); }
        std::string decode(bool x) { return "b" + std::to_string(x); }
    };

    MyDuck x{Codec{}};

    EXPECT_EQ(x.encode(5),     10);
    EXPECT_EQ(x.encode(true),   6);
    EXPECT_EQ(x.decode(7),     "i7");
    EXPECT_EQ(x.decode(false), "b0");
}

// ============================================================================
// Operator overloading (op_plus, both sides)
// ============================================================================

struct AdderPolicy {
    auto operator+(int)    const -> int;
    auto operator+(double) const -> double;
};

TEST(DuckOperators, PlusLhsOverloads) {
    // Two lhs overloads of operator+ with different rhs types.
    using MyDuck = rjk::duck<AdderPolicy>;

    struct Adder {
        int    operator+(int rhs)    const { return 10 + rhs; }
        double operator+(double rhs) const { return 10.0 + rhs; }
    };

    MyDuck x{Adder{}};

    EXPECT_EQ(x + 5,   15);
    EXPECT_EQ(x + 2.5, 12.5);
}

struct Addend {
    int value = 5;
};

int    operator+(int lhs,    const Addend& rhs) { return lhs + rhs.value; }
double operator+(double lhs, const Addend& rhs) { return lhs + rhs.value; }

TEST(DuckOperators, PlusRhsOverloads) {
    struct MyPolicy {
        [[=rjk::right_side]]
        int operator+(int) const;

        [[=rjk::right_side]]
        double operator+(double) const;
    };

    rjk::duck<MyPolicy> x{Addend{5}};

    EXPECT_EQ(3 + x,   8);
    EXPECT_EQ(1.5 + x, 6.5);
}

struct Symmetric {
    int value = 7;
    int operator+(int rhs) const { return value + rhs; }
};
int operator+(int lhs, const Symmetric& rhs) { return lhs + rhs.value; }

TEST(DuckOperators, PlusBothSides) {
    struct AddableLhs {
        int operator+(int) const;
    };
    struct AddableRhs {
        [[=rjk::right_side]]
        int operator+(int) const;
    };
    rjk::duck<AddableLhs, AddableRhs> x{Symmetric{7}};

    EXPECT_EQ(x + 3, 10);
    EXPECT_EQ(3 + x, 10);
}

// ============================================================================
// Mixed has_fn and has_op on the same duck
// ============================================================================

struct ScalerPolicy {
    auto scale(int)    const -> int;
    auto scale(double) const -> double;
    auto operator+(int) const -> int;
};

TEST(DuckOverloading, MixedFnAndOp) {
    using MyDuck = rjk::duck<ScalerPolicy>;

    struct Scaler {
        int    scale(int x)    const { return x * 2; }
        double scale(double x) const { return x * 2.0; }
        int    operator+(int rhs) const { return 100 + rhs; }
    };

    MyDuck x{Scaler{}};

    EXPECT_EQ(x.scale(3),   6);
    EXPECT_EQ(x.scale(1.5), 3.0);
    EXPECT_EQ(x + 1,        101);
}

// ============================================================================
// Swap underlying type after construction (regression for overload sets)
// ============================================================================

TEST(DuckOverloading, SwapUnderlyingType) {
    using MyDuck = rjk::duck<ComputePolicy>;

    struct DoubleComputer {
        int    compute(int x)    { return x * 2; }
        double compute(double x) { return x * 2.0; }
    };

    struct TripleComputer {
        int    compute(int x)    { return x * 3; }
        double compute(double x) { return x * 3.0; }
    };

    MyDuck x{DoubleComputer{}};
    EXPECT_EQ(x.compute(4),   8);
    EXPECT_EQ(x.compute(2.0), 4.0);

    x = TripleComputer{};
    EXPECT_EQ(x.compute(4),   12);
    EXPECT_EQ(x.compute(2.0), 6.0);
}

// ============================================================================
// Overloads that differ only in argument const-ness/ref-qualification
// ============================================================================

struct StorePolicy {
    auto store(int&)       -> void;
    auto store(const int&) -> void;
};

TEST(DuckOverloading, ConstRefArgOverloads) {
    using MyDuck = rjk::duck<StorePolicy>;

    struct Store {
        int last_mutable = 0;
        int last_const   = 0;
        void store(int& x)       { last_mutable = x; x = 99; }
        void store(const int& x) { last_const   = x; }
    };

    MyDuck x{std::in_place_type<Store>};

    int mval = 5;
    x.store(mval);
    EXPECT_EQ(mval, 99); // mutable overload modified it

    const int cval = 7;
    x.store(cval);
    EXPECT_EQ(rjk::get<Store>(x).last_const, 7);
}

// ============================================================================
// Overload set with noexcept variants
// ============================================================================

TEST(DuckOverloading, NoexceptOverloads) {
    struct Runnable {
        int run(int);
        int run(double);
    };
    struct Runner {
        int run(int x)    noexcept { return x + 1; }
        int run(double x) noexcept { return static_cast<int>(x) + 2; }
    };

    rjk::duck<Runnable> x{Runner{}};

    EXPECT_EQ(x.run(10),   11);
    EXPECT_EQ(x.run(10.0), 12);
}

TEST(DuckOverloading, Inheritance) {
    struct Policy {
        int foo() const;
        int bar() const;
    };

    struct A { int foo() const { return 5; } };
    struct B : A { int bar() const { return 10; } };

    rjk::duck<Policy> d{B{}};
    EXPECT_EQ(d.foo(), 5);
    EXPECT_EQ(d.bar(), 10);
}

struct OverloadTestPolicy {
    int foo(int) const;
};

TEST(DuckOverloading, StaticFunction) {
    struct A {
        static int foo(int) { return 5; }
        static int foo(double) { return 5; }
    };

    rjk::duck<OverloadTestPolicy> d{A{}};
    EXPECT_EQ(d.foo(0), 5);
}

TEST(DuckOverloading, ExplicitObjectParam) {
    struct A {
        int foo(this const A&, int) { return 5; }

        int foo(double) const { return 10; }
    };

    rjk::duck<OverloadTestPolicy> d{A{}};
    EXPECT_EQ(d.foo(0), 5);
}

TEST(DuckOverloading, DerivedDeducingThis) {
    struct B {};

    struct A : B {
        int foo(this const B&, int) { return 5; }
    };

    rjk::duck<OverloadTestPolicy> d{A{}};
    EXPECT_EQ(d.foo(0), 5);
}

TEST(DuckOverloading, ReferenceWrapperConversion) {
    struct MutablePolicy {
        int foo(int);
    };

    struct A {
        int foo(this std::reference_wrapper<A>, int) { return 5; }
        int foo(this A&&, int) { return 10; }
    };

    rjk::duck<MutablePolicy> d{A{}};
    EXPECT_EQ(d.foo(0), 5);
}

TEST(DuckOverloading, FunctionPointer) {
    struct A {
        int (*foo)(int);
    };

    rjk::duck<OverloadTestPolicy> d{A{.foo = [](int) { return 5; }}};
    EXPECT_EQ(d.foo(0), 5);
}

struct StaticA {
    constexpr static int (*foo)(int) = [](int) { return 5; };
};
static_assert(rjk::satisfies<StaticA, OverloadTestPolicy>);

TEST(DuckOverloading, DefaultParameter) {
    struct A {
        int foo(int, int x = 5) const { return x; }
    };

    rjk::duck<OverloadTestPolicy> d{A{}};
    EXPECT_EQ(d.foo(0), 5);
}

TEST(DuckOverloading, MutlipleDefaultParams) {
    struct MultiDefault {
        int foo(int);
        int foo(int, int);
    };

    struct A {
        int foo(int, int x = 5) { return x; }
    };

    rjk::duck<MultiDefault> d{A{}};
    EXPECT_EQ(d.foo(0), 5);
    EXPECT_EQ(d.foo(0, 10), 10);
}

struct StaticDefault {
    static int foo(int, int = 0) { return 5; }
};
static_assert(rjk::satisfies<StaticDefault, OverloadTestPolicy>);

// Regression test: default parameters do not work with explicit object parameters
struct ExplicitObjDefault {
    int foo(this const ExplicitObjDefault&, int, int = 0);
};
static_assert(!rjk::satisfies<ExplicitObjDefault, OverloadTestPolicy>);

} // namespace rjk_test