#include "rjk/duck.hpp"
#include "test_fixtures.hpp"

#include <gtest/gtest.h>

namespace rjk_test {
TEST(DuckEmplace, EmplaceBasic) {
    TestDuck x{B{}};
    x.emplace<A>();
    EXPECT_EQ(x.other('a'), 10);
}

TEST(DuckEmplace, EmplaceReturnsRef) {
    TestDuck x{B{}};
    auto& ref = x.emplace<B>();
    EXPECT_EQ(ref.other('a'), 3);
}

TEST(DuckEmplace, EmplaceInitializerList) {
    struct FromIL {
        int sum = 0;

        FromIL(std::initializer_list<int> il) {
            for (int v : il)
                sum += v;
        }

        void test() {}

        int other(char) { return sum; }
    };
    TestDuck x{A{}};
    x.emplace<FromIL>({10, 20});
    EXPECT_EQ(x.other('a'), 30);
}

// ── Swap ─────────────────────────────────────────────────────────────────────

TEST(DuckSwap, SwapSBOWithSBO) {
    TestDuck x{A{}};
    TestDuck y{B{}};
    std::swap(x, y);
    EXPECT_EQ(x.other('a'), 3);
    EXPECT_EQ(y.other('a'), 10);
}

TEST(DuckSwap, SwapHeapWithHeap) {
    BigDuck x{Big{}};
    BigDuck y{Big{}};
    // give them distinguishable return values via emplace trick — use B on heap via padding
    // Just verify no crash and dispatch still works
    std::swap(x, y);
    EXPECT_EQ(x.other('a'), 99);
}

TEST(DuckSwap, SwapSBOWithHeap) {
    BigDuck x{A{}}; // SBO
    BigDuck y{Big{}}; // heap
    std::swap(x, y);
    EXPECT_EQ(x.other('a'), 99);
    EXPECT_EQ(y.other('a'), 10);
}

TEST(DuckSwap, SwapHeapWithSBO) {
    BigDuck x{Big{}}; // heap
    BigDuck y{A{}}; // SBO
    std::swap(x, y);
    EXPECT_EQ(x.other('a'), 10);
    EXPECT_EQ(y.other('a'), 99);
}

// ── get / get_if ─────────────────────────────────────────────────────────────

TEST(DuckGet, GetLvalue) {
    TestDuck x{A{}};
    EXPECT_NO_THROW(rjk::get<A>(x));
}

TEST(DuckGet, GetConstLvalue) {
    const TestDuck x{A{}};
    EXPECT_NO_THROW(rjk::get<A>(x));
}

TEST(DuckGet, GetRvalue) {
    TestDuck x{A{}};
    EXPECT_NO_THROW(rjk::get<A>(std::move(x)));
}

TEST(DuckGet, GetConstRvalue) {
    const TestDuck x{A{}};
    EXPECT_NO_THROW(rjk::get<A>(std::move(x)));
}

TEST(DuckGet, GetWrongTypeThrows) {
    TestDuck x{A{}};
    EXPECT_THROW(rjk::get<B>(x), rjk::bad_duck_access);
}

TEST(DuckGet, GetIfCorrectType) {
    TestDuck x{A{}};
    EXPECT_NE(rjk::get_if<A>(x), nullptr);
}

TEST(DuckGet, GetIfWrongType) {
    TestDuck x{A{}};
    EXPECT_EQ(rjk::get_if<B>(x), nullptr);
}

TEST(DuckGet, GetIfConst) {
    const TestDuck x{A{}};
    EXPECT_NE(rjk::get_if<A>(x), nullptr);
    EXPECT_EQ(rjk::get_if<B>(x), nullptr);
}

TEST(DuckGet, GetRvalueThrowsOnWrongType) {
    TestDuck x{A{}};
    EXPECT_THROW(rjk::get<B>(std::move(x)), rjk::bad_duck_access);
}

TEST(DuckGet, GetConstRvalueThrowsOnWrongType) {
    const TestDuck x{A{}};
    EXPECT_THROW(rjk::get<B>(std::move(x)), rjk::bad_duck_access);
}

TEST(DuckQualifiers, ConstMethod) {
    struct Policy {
        int doSmth() const;
    };

    rjk::duck<rjk::policy<rjk::has_fn<"doSmth", int() const>> > x{WithConst{}};
    EXPECT_EQ(x.doSmth(), 42);
}

TEST(DuckQualifiers, ConstMethodOnConstDuck) {
    const rjk::duck<rjk::policy<rjk::has_fn<"doSmth", int() const>> > x{WithConst{}};
    EXPECT_EQ(x.doSmth(), 42);
}

TEST(DuckQualifiers, LvalueRefMethod) {
    rjk::duck<rjk::policy<rjk::has_fn<"lvalue_fn", int() &>> > x{WithLvalueRef{}};
    EXPECT_EQ(x.lvalue_fn(), 1);
}

TEST(DuckQualifiers, RvalueRefMethod) {
    rjk::duck<rjk::policy<rjk::has_fn<"rvalue_fn", int() &&>> > x{WithRvalueRef{}};
    EXPECT_EQ(std::move(x).rvalue_fn(), 2);
}

TEST(DuckQualifiers, LvalueReturn) {
    using MyDuck = rjk::duck<rjk::policy<rjk::has_fn<"lvalue_ret", int&()>>>;

    struct TestStruct {
        int x = 10;

        int& lvalue_ret() { return x; }
    };

    MyDuck x{TestStruct{}};
    EXPECT_EQ(x.lvalue_ret(), 10);
    x.lvalue_ret() = 5;
    EXPECT_EQ(x.lvalue_ret(), 5);
}

TEST(DuckQualifiers, Noexcept) {
    struct [[=rjk::trait]] TestTrait {
        int foo() noexcept;
    };

    struct TestA {
        int foo() noexcept { return 10; }
    };

    struct TestB {
        int foo() { return 20; }
    };

    rjk::duck<TestTrait> d{TestA{}};
    EXPECT_EQ(d.foo(), 10);
    static_assert(noexcept(d.foo()));
}

}
