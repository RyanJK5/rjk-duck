#include "duck.hpp"
#include "test_fixtures.hpp"

#include <gtest/gtest.h>

namespace rjk_test {
TEST(AnyEmplace, EmplaceBasic) {
    TestAny x{B{}};
    x.emplace<A>();
    EXPECT_EQ(x->other('a'), 10);
}

TEST(AnyEmplace, EmplaceReturnsRef) {
    TestAny x{B{}};
    auto& ref = x.emplace<B>();
    EXPECT_EQ(ref.other('a'), 3);
}

TEST(AnyEmplace, EmplaceInitializerList) {
    struct FromIL {
        int sum = 0;

        FromIL(std::initializer_list<int> il) {
            for (int v : il)
                sum += v;
        }

        void test() {}

        int other(char) { return sum; }
    };
    TestAny x{A{}};
    x.emplace<FromIL>({10, 20});
    EXPECT_EQ(x->other('a'), 30);
}

// ── Reset ────────────────────────────────────────────────────────────────────

TEST(AnyReset, Reset) {
    TestAny x{A{}};
    EXPECT_TRUE(x.has_value());
    x.reset();
    EXPECT_FALSE(x.has_value());
}

TEST(AnyReset, ResetRunsDestructor) {
    A::instance_count = 0;
    {
        TestAny x{A{}};
        EXPECT_EQ(A::instance_count, 1);
        x.reset();
        EXPECT_EQ(A::instance_count, 0);
    }
}

TEST(AnyReset, ResetEmpty) {
    TestAny x{};
    EXPECT_NO_THROW(x.reset());
    EXPECT_FALSE(x.has_value());
}

// ── Swap ─────────────────────────────────────────────────────────────────────

TEST(AnySwap, SwapSBOWithSBO) {
    TestAny x{A{}};
    TestAny y{B{}};
    x.swap(y);
    EXPECT_EQ(x->other('a'), 3);
    EXPECT_EQ(y->other('a'), 10);
}

TEST(AnySwap, SwapHeapWithHeap) {
    BigAny x{Big{}};
    BigAny y{Big{}};
    // give them distinguishable return values via emplace trick — use B on heap via padding
    // Just verify no crash and dispatch still works
    x.swap(y);
    EXPECT_EQ(x->other('a'), 99);
}

TEST(AnySwap, SwapSBOWithHeap) {
    BigAny x{A{}}; // SBO
    BigAny y{Big{}}; // heap
    x.swap(y);
    EXPECT_EQ(x->other('a'), 99);
    EXPECT_EQ(y->other('a'), 10);
}

TEST(AnySwap, SwapHeapWithSBO) {
    BigAny x{Big{}}; // heap
    BigAny y{A{}}; // SBO
    x.swap(y);
    EXPECT_EQ(x->other('a'), 10);
    EXPECT_EQ(y->other('a'), 99);
}

// ── get / get_if ─────────────────────────────────────────────────────────────

TEST(AnyGet, GetLvalue) {
    TestAny x{A{}};
    EXPECT_NO_THROW(x.get<A>());
}

TEST(AnyGet, GetConstLvalue) {
    const TestAny x{A{}};
    EXPECT_NO_THROW(x.get<A>());
}

TEST(AnyGet, GetRvalue) {
    TestAny x{A{}};
    EXPECT_NO_THROW(std::move(x).get<A>());
}

TEST(AnyGet, GetConstRvalue) {
    const TestAny x{A{}};
    EXPECT_NO_THROW(std::move(x).get<A>());
}

TEST(AnyGet, GetWrongTypeThrows) {
    TestAny x{A{}};
    EXPECT_THROW(x.get<B>(), rjk::bad_duck_access);
}

TEST(AnyGet, GetIfCorrectType) {
    TestAny x{A{}};
    EXPECT_NE(x.get_if<A>(), nullptr);
}

TEST(AnyGet, GetIfWrongType) {
    TestAny x{A{}};
    EXPECT_EQ(x.get_if<B>(), nullptr);
}

TEST(AnyGet, GetIfConst) {
    const TestAny x{A{}};
    EXPECT_NE(x.get_if<A>(), nullptr);
    EXPECT_EQ(x.get_if<B>(), nullptr);
}

TEST(AnyGet, GetRvalueThrowsOnWrongType) {
    TestAny x{A{}};
    EXPECT_THROW(std::move(x).get<B>(), rjk::bad_duck_access);
}

TEST(AnyGet, GetConstRvalueThrowsOnWrongType) {
    const TestAny x{A{}};
    EXPECT_THROW(std::move(x).get<B>(), rjk::bad_duck_access);
}

// ── operator-> / operator* ───────────────────────────────────────────────────

TEST(AnyAccess, ArrowOperator) {
    TestAny x{A{}};
    EXPECT_NO_THROW(x->test());
}

TEST(AnyAccess, DerefOperator) {
    TestAny x{A{}};
    auto& vt = *x;
    EXPECT_EQ(vt.other('a'), 10);
}

// ── Const-qualified methods ───────────────────────────────────────────────────

TEST(AnyQualifiers, ConstMethod) {
    rjk::duck<rjk::has_fn<"get", int() const> > x{WithConst{}};
    EXPECT_EQ(x->get(), 42);
}

TEST(AnyQualifiers, ConstMethodOnConstAny) {
    const rjk::duck<rjk::has_fn<"get", int() const> > x{WithConst{}};
    EXPECT_EQ(x->get(), 42);
}

// ── Ref-qualified methods ─────────────────────────────────────────────────────

TEST(AnyQualifiers, LvalueRefMethod) {
    rjk::duck<rjk::has_fn<"lvalue_fn", int() &> > x{WithLvalueRef{}};
    EXPECT_EQ(x->lvalue_fn(), 1);
}

TEST(AnyQualifiers, RvalueRefMethod) {
    rjk::duck<rjk::has_fn<"rvalue_fn", int() &&> > x{WithRvalueRef{}};
    EXPECT_EQ((*std::move(x)).rvalue_fn(), 2);
}
}
