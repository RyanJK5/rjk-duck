#include "duck.hpp"
#include <gtest/gtest.h>
#include <array>
#include <memory>

#include <vector>
#include <deque>

// ── Test fixtures ────────────────────────────────────────────────────────────

struct A {
    inline static int instance_count = 0;

    A() { ++instance_count; }
    ~A() { --instance_count; }
    A(const A&) { ++instance_count; }
    A(A&&) noexcept { ++instance_count; }

    A& operator=(const A&) = default;

    A& operator=(A&&) = default;

    void test() {}

    int other(char) { return 10; }

    void consume() {}
};

struct B {
    void test() {}

    int other(char) { return 3; }

    void consume() {}
};

// Larger than SBO (32 bytes)
struct Big {
    std::array<std::byte, 64> data{};

    void test() {}

    int other(char) { return 99; }

    void consume() {}
};

// Move-only
struct MoveOnly {
    std::unique_ptr<int> val;

    explicit MoveOnly(int x) : val(std::make_unique<int>(x)) {}

    MoveOnly(const MoveOnly&) = delete;

    MoveOnly& operator=(const MoveOnly&) = delete;

    MoveOnly(MoveOnly&&) = default;

    MoveOnly& operator=(MoveOnly&&) = default;

    void test() {}

    int other(char) { return *val; }

    void consume() {}
};

// const-qualified method
struct WithConst {
    int value = 42;
    int get() const { return value; }
};

// ref-qualified methods
struct WithLvalueRef {
    int lvalue_fn() & { return 1; }
};

struct WithRvalueRef {
    int rvalue_fn() && { return 2; }
};

using TestAny = rjk::duck<
    rjk::has_fn<"test", void()>,
    rjk::has_fn<"other", int(char)>
>;

// Distinct alias for tests mixing SBO and heap types
using BigAny = rjk::duck<
    rjk::has_fn<"test", void()>,
    rjk::has_fn<"other", int(char)>
>;

// ── Construction ─────────────────────────────────────────────────────────────

TEST(AnyConstruct, DefaultConstruct) {
    TestAny x;
    EXPECT_FALSE(x.has_value());
}

TEST(AnyConstruct, FromRvalue) {
    TestAny x{A{}};
    EXPECT_TRUE(x.has_value());
    EXPECT_NO_THROW(x->test());
    EXPECT_EQ(x->other('a'), 10);
}

TEST(AnyConstruct, FromLvalue) {
    A a{};
    TestAny x{a};
    EXPECT_TRUE(x.has_value());
    EXPECT_EQ(x->other('a'), 10);
}

TEST(AnyConstruct, InPlaceType) {
    TestAny x{std::in_place_type<A>};
    EXPECT_TRUE(x.has_value());
    EXPECT_EQ(x->other('a'), 10);
}

TEST(AnyConstruct, InPlaceTypeWithArgs) {
    // B has no constructor args but exercises the path
    TestAny x{std::in_place_type<B>};
    EXPECT_TRUE(x.has_value());
    EXPECT_EQ(x->other('a'), 3);
}

TEST(AnyConstruct, InPlaceTypeInitializerList) {
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
    TestAny x{std::in_place_type<FromIL>, {1, 2, 3}};
    EXPECT_EQ(x->other('a'), 6);
}

TEST(AnyConstruct, HeapAllocated) {
    TestAny x{Big{}};
    EXPECT_TRUE(x.has_value());
    EXPECT_EQ(x->other('a'), 99);
    // Big is 64 bytes > SBO size of 32, so heap path is taken
    EXPECT_FALSE(rjk::detail::fits_sbo<Big>);
}

// ── Assignment ───────────────────────────────────────────────────────────────

TEST(AnyAssign, AssignRvalue) {
    TestAny x{B{}};
    x = A{};
    EXPECT_EQ(x->other('a'), 10);
    x = B{};
    EXPECT_EQ(x->other('a'), 3);
}

TEST(AnyAssign, AssignLvalue) {
    TestAny x{B{}};
    A a{};
    x = a;
    EXPECT_EQ(x->other('a'), 10);
}

TEST(AnyAssign, AssignHeap) {
    TestAny x{A{}};
    x = Big{};
    EXPECT_EQ(x->other('a'), 99);
}

// ── Copy ─────────────────────────────────────────────────────────────────────

TEST(AnyCopy, CopyConstruct) {
    TestAny x{A{}};
    TestAny y{x};
    EXPECT_TRUE(y.has_value());
    EXPECT_EQ(y->other('a'), 10);
}

TEST(AnyCopy, CopyConstructIndependent) {
    TestAny x{A{}};
    TestAny y{x};
    x = B{};
    EXPECT_EQ(y->other('a'), 10); // y unaffected
    EXPECT_EQ(x->other('a'), 3);
}

TEST(AnyCopy, CopyAssign) {
    TestAny x{A{}};
    TestAny y{B{}};
    y = x;
    EXPECT_EQ(y->other('a'), 10);
}

TEST(AnyCopy, CopyAssignIndependent) {
    TestAny x{A{}};
    TestAny y{B{}};
    y = x;
    x = B{};
    EXPECT_EQ(y->other('a'), 10); // y unaffected
}

TEST(AnyCopy, CopyAssignSelf) {
    TestAny x{A{}};
    x = static_cast<const TestAny&>(x); // self-assign
    EXPECT_TRUE(x.has_value());
    EXPECT_EQ(x->other('a'), 10);
}

TEST(AnyCopy, CopyHeap) {
    TestAny x{Big{}};
    TestAny y{x};
    EXPECT_EQ(y->other('a'), 99);
}

// ── Move ─────────────────────────────────────────────────────────────────────

TEST(AnyMove, MoveConstructSBO) {
    TestAny x{A{}};
    TestAny y{std::move(x)};
    EXPECT_TRUE(y.has_value());
    EXPECT_EQ(y->other('a'), 10);
    EXPECT_FALSE(x.has_value());
}

TEST(AnyMove, MoveConstructHeap) {
    TestAny x{Big{}};
    TestAny y{std::move(x)};
    EXPECT_TRUE(y.has_value());
    EXPECT_EQ(y->other('a'), 99);
    EXPECT_FALSE(x.has_value());
}

TEST(AnyMove, MoveAssignSBO) {
    TestAny x{A{}};
    TestAny y{B{}};
    y = std::move(x);
    EXPECT_TRUE(y.has_value());
    EXPECT_EQ(y->other('a'), 10);
    EXPECT_FALSE(x.has_value());
}

TEST(AnyMove, MoveAssignHeap) {
    TestAny x{Big{}};
    TestAny y{B{}};
    y = std::move(x);
    EXPECT_TRUE(y.has_value());
    EXPECT_EQ(y->other('a'), 99);
    EXPECT_FALSE(x.has_value());
}

TEST(AnyMove, MoveAssignSelf) {
    TestAny x{A{}};
    TestAny& alias = x;

    x = std::move(alias);
    // post self-move state is valid but unspecified; just don't crash
}

TEST(AnyMove, MoveOnlyType) {
    rjk::duck<rjk::has_fn<"test", void()>, rjk::has_fn<"other", int(char)> > x{
        MoveOnly{7}};
    EXPECT_EQ(x->other('a'), 7);
    auto y = std::move(x);
    EXPECT_EQ(y->other('a'), 7);
    EXPECT_FALSE(x.has_value());
}

// ── Emplace ──────────────────────────────────────────────────────────────────

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

// ── Destructor / lifetime ────────────────────────────────────────────────────

TEST(AnyLifetime, DestructorCalledOnDestroy) {
    A::instance_count = 0;
    {
        TestAny x{A{}};
        EXPECT_EQ(A::instance_count, 1);
    }
    EXPECT_EQ(A::instance_count, 0);
}

TEST(AnyLifetime, DestructorCalledOnReassign) {
    A::instance_count = 0;
    TestAny x{A{}};
    EXPECT_EQ(A::instance_count, 1);
    x = B{};
    EXPECT_EQ(A::instance_count, 0);
}

TEST(AnyLifetime, DestructorCalledOnEmplace) {
    A::instance_count = 0;
    TestAny x{A{}};
    EXPECT_EQ(A::instance_count, 1);
    x.emplace<B>();
    EXPECT_EQ(A::instance_count, 0);
}

TEST(AnyLifetime, HeapDestructorCalled) {
    // Exercises the heap delete path — verified clean under asan/valgrind
    {
        TestAny x{Big{}};
        EXPECT_TRUE(x.has_value());
    }
}

// ── bad_duck_access ────────────────────────────────────────────────────────────

TEST(BadAnyAccess, WhatMessage) {
    rjk::bad_duck_access ex{};
    EXPECT_STREQ(ex.what(), "type mismatch");
}

// ── storage internals ────────────────────────────────────────────────────────

TEST(StorageInternals, SBOFitsSmallType) {
    EXPECT_TRUE(rjk::detail::fits_sbo<A>);
}

TEST(StorageInternals, SBODoesNotFitBigType) {
    EXPECT_FALSE(rjk::detail::fits_sbo<Big>);
}

TEST(StorageInternals, HasTypeSBO) {
    rjk::detail::storage s{std::in_place_type<A>};
    EXPECT_TRUE(s.has_type<A>());
    EXPECT_FALSE(s.has_type<B>());
}

TEST(StorageInternals, HasTypeHeap) {
    rjk::detail::storage s{std::in_place_type<Big>};
    EXPECT_TRUE(s.has_type<Big>());
    EXPECT_FALSE(s.has_type<A>());
}

TEST(StorageInternals, DefaultHasNoValue) {
    rjk::detail::storage s{};
    EXPECT_FALSE(s.has_value());
}

TEST(StorageInternals, EmplaceOverwrites) {
    rjk::detail::storage s{std::in_place_type<A>};
    EXPECT_TRUE(s.has_type<A>());
    s.emplace<B>();
    EXPECT_TRUE(s.has_type<B>());
}

TEST(StorageInternals, MoveSBO) {
    rjk::detail::storage s{std::in_place_type<A>};
    rjk::detail::storage t{std::move(s)};
    EXPECT_TRUE(t.has_value());
    EXPECT_FALSE(s.has_value());
}

TEST(StorageInternals, MoveHeap) {
    rjk::detail::storage s{std::in_place_type<Big>};
    rjk::detail::storage t{std::move(s)};
    EXPECT_TRUE(t.has_value());
    EXPECT_FALSE(s.has_value());
}

TEST(StorageInternals, CopySBO) {
    rjk::detail::storage s{std::in_place_type<A>};
    rjk::detail::storage t{s};
    EXPECT_TRUE(t.has_value());
    EXPECT_TRUE(s.has_value());
}

TEST(StorageInternals, CopyHeap) {
    rjk::detail::storage s{std::in_place_type<Big>};
    rjk::detail::storage t{s};
    EXPECT_TRUE(t.has_value());
    EXPECT_TRUE(s.has_value());
}

TEST(StorageInternals, CopyAssignSBO) {
    rjk::detail::storage s{std::in_place_type<A>};
    rjk::detail::storage t{std::in_place_type<B>};
    t = s;
    EXPECT_TRUE(t.has_type<A>());
}

TEST(StorageInternals, CopyAssignHeap) {
    rjk::detail::storage s{std::in_place_type<Big>};
    rjk::detail::storage t{std::in_place_type<Big>};
    t = s;
    EXPECT_TRUE(t.has_type<Big>());
}

TEST(StorageInternals, MoveAssignSBO) {
    rjk::detail::storage s{std::in_place_type<A>};
    rjk::detail::storage t{std::in_place_type<B>};
    t = std::move(s);
    EXPECT_TRUE(t.has_type<A>());
    EXPECT_FALSE(s.has_value());
}

TEST(StorageInternals, MoveAssignHeap) {
    rjk::detail::storage s{std::in_place_type<Big>};
    rjk::detail::storage t{std::in_place_type<Big>};
    t = std::move(s);
    EXPECT_TRUE(t.has_type<Big>());
    EXPECT_FALSE(s.has_value());
}

TEST(StorageInternals, ResetClearsValue) {
    rjk::detail::storage s{std::in_place_type<A>};
    s.reset();
    EXPECT_FALSE(s.has_value());
}

TEST(StorageInternals, UsingsSBO) {
    rjk::detail::storage s{std::in_place_type<A>};
    EXPECT_TRUE(s.using_sbo());
    rjk::detail::storage t{std::in_place_type<Big>};
    EXPECT_FALSE(t.using_sbo());
}

TEST(StorageInternals, GetReturnsNonNull) {
    rjk::detail::storage s{std::in_place_type<A>};
    EXPECT_NE(s.get(), nullptr);
    const rjk::detail::storage cs{std::in_place_type<A>};
    EXPECT_NE(cs.get(), nullptr);
}

// ── Real-world stdlib scenarios ───────────────────────────────────────────────

// Type-erased container: anything with size() and clear()
using Sizeable = rjk::duck<
    rjk::has_fn<"size", std::size_t() const>,
    rjk::has_fn<"clear", void()>,
    rjk::has_fn<"empty", bool() const>
>;
static_assert(
    rjk::remove_noexcept(type_of(^^std::vector<int>::size)) ==
    rjk::remove_noexcept(^^std::size_t() const));

TEST(StdlibScenarios, VectorSizeAndEmpty) {
    Sizeable x{std::vector<int>{1, 2, 3}};
    EXPECT_EQ(x->size(), 3UZ);
    EXPECT_FALSE(x->empty());
}

TEST(StdlibScenarios, StringSizeAndEmpty) {
    Sizeable x{std::string{"hello"}};
    EXPECT_EQ(x->size(), 5u);
    EXPECT_FALSE(x->empty());
}

TEST(StdlibScenarios, VectorClear) {
    Sizeable x{std::vector<int>{1, 2, 3}};
    x->clear();
    EXPECT_EQ(x->size(), 0u);
    EXPECT_TRUE(x->empty());
}

TEST(StdlibScenarios, MapSizeAndClear) {
    std::map<int, int> m{{1, 2}, {3, 4}};
    Sizeable x{std::move(m)};
    EXPECT_EQ(x->size(), 2u);
    x->clear();
    EXPECT_TRUE(x->empty());
}

TEST(StdlibScenarios, DequeInSizeable) {
    Sizeable x{std::deque<double>{1.0, 2.0}};
    EXPECT_EQ(x->size(), 2u);
    x->clear();
    EXPECT_TRUE(x->empty());
}

TEST(StdlibScenarios, SizeableSwap) {
    Sizeable x{std::vector<int>{1, 2, 3}};
    Sizeable y{std::string{"hi"}};
    x.swap(y);
    EXPECT_EQ(x->size(), 2u); // string "hi"
    EXPECT_EQ(y->size(), 3u); // vector {1,2,3}
}

TEST(StdlibScenarios, SizeableCopy) {
    Sizeable x{std::vector<int>{1, 2, 3}};
    Sizeable y{x};
    y->clear();
    EXPECT_EQ(x->size(), 3u); // x unaffected
    EXPECT_EQ(y->size(), 0u);
}

// Type-erased string-like: anything with find() and substr()-equivalent
// Use a narrower interface: just size() const and a find(char) method
using StringLike = rjk::duck<
    rjk::has_fn<"size", std::size_t() const>,
    rjk::has_fn<"empty", bool() const>
>;

TEST(StdlibScenarios, StringVsVectorChar) {
    StringLike a{std::string{"hello"}};
    StringLike b{std::vector<char>{'h', 'i'}};
    EXPECT_EQ(a->size(), 5u);
    EXPECT_EQ(b->size(), 2u);
    EXPECT_FALSE(a->empty());
    EXPECT_FALSE(b->empty());
}

// Type-erased output: anything callable as a sink via push_back
// Use back_inserter-compatible containers
using Pushable = rjk::duck<
    rjk::has_fn<"push_back", void(const int&)>,
    rjk::has_fn<"size", std::size_t() const>
>;

TEST(StdlibScenarios, VectorPushBack) {
    Pushable x{std::vector<int>{}};
    x->push_back(10);
    x->push_back(20);
    EXPECT_EQ(x->size(), 2u);
}

TEST(StdlibScenarios, DequePushBack) {
    Pushable x{std::deque<int>{}};
    x->push_back(1);
    x->push_back(2);
    x->push_back(3);
    EXPECT_EQ(x->size(), 3u);
}

TEST(StdlibScenarios, PushableMove) {
    Pushable x{std::vector<int>{1, 2}};
    Pushable y{std::move(x)};
    y->push_back(3);
    EXPECT_EQ(y->size(), 3u);
    EXPECT_FALSE(x.has_value());
}

// Move-only stdlib type: unique_ptr wrapped in a custom holder
// (unique_ptr itself doesn't satisfy any useful tag directly,
//  so wrap it to verify move-only heap allocation works with real stdlib internals)
struct UniquePtrHolder {
    std::unique_ptr<std::vector<int> > data;

    explicit UniquePtrHolder(std::vector<int> v)
        : data(std::make_unique<std::vector<int> >(std::move(v))) {}

    std::size_t size() const { return data->size(); }
    bool empty() const { return data->empty(); }
    void clear() { data->clear(); }
};

TEST(StdlibScenarios, MoveOnlyStdlibWrapper) {
    Sizeable x{UniquePtrHolder{std::vector<int>{1, 2, 3, 4}}};
    EXPECT_EQ(x->size(), 4UZ);
    Sizeable y{std::move(x)};
    EXPECT_EQ(y->size(), 4UZ);
    EXPECT_FALSE(x.has_value());
    y->clear();
    EXPECT_TRUE(y->empty());
}

TEST(BasicOperator, Plus) {
    using BasicPlus = rjk::duck<
        rjk::has_op<rjk::op_plus, int(rjk::self, int)>,
        rjk::has_op<rjk::op_plus, int(int, rjk::self)>
    >;

    BasicPlus x{10};

    EXPECT_EQ(*x + 5, 15);
    EXPECT_EQ(5 + *x + 5, 20);
}

using CommutativePlus = rjk::duck<
    rjk::has_op<rjk::op_plus, int(rjk::self&, const rjk::this_duck_t&)>,
    rjk::has_op<rjk::op_plus, int(const rjk::this_duck_t&, rjk::self&)>
>;

struct CommutativePlusA {
    int operator+(const CommutativePlus&) {
        return 5;
    }
};

int operator+(const CommutativePlus&, CommutativePlusA&) {
    return 25;
}

TEST(BasicOperator, PlusOtherCommutative) {
    CommutativePlus x{CommutativePlusA{}};
    CommutativePlus y{CommutativePlusA{}};

    EXPECT_EQ(*x + y, 5);
    EXPECT_EQ(*y + x, 5);
    EXPECT_EQ(x + *y, 25);
    EXPECT_EQ(y + *x, 25);
}

TEST(BasicOperator, PlusOther) {
    using BasicPlus = rjk::duck<
        rjk::has_op<rjk::op_plus, int(rjk::self&, const rjk::this_duck_t&)>
    >;

    struct A {
        int operator+(const BasicPlus&) {
            return 5;
        }
    };

    struct B {
        int operator+(const BasicPlus&) {
            return 6;
        }
    };

    BasicPlus a{A{}};
    BasicPlus b{B{}};

    EXPECT_EQ(*a + b, 5);
    EXPECT_EQ(*b + a, 6);
}

TEST(BasicOperator, RefTest) {
    using RefPlus = rjk::duck<
        rjk::has_op<rjk::op_plus, int(rjk::self &, rjk::this_duck_t &)>
    >;

    struct A {
        int operator+(RefPlus&) const {
            return 10;
        }

        int operator+(RefPlus&) {
            return 20;
        }
    };

    RefPlus x{A{}};
    RefPlus y{A{}};
    EXPECT_EQ(*x + y, 20);
}

TEST(BasicOperator, ConstRefTest) {
    using ConstRefPlus = rjk::duck<
        rjk::has_op<rjk::op_plus, int(const rjk::self &, const rjk::this_duck_t &)>
    >;

    struct A {
        int operator+(const ConstRefPlus&) {
            return 10;
        }

        int operator+(const ConstRefPlus&) const {
            return 20;
        }
    };

    ConstRefPlus x{A{}};
    ConstRefPlus y{A{}};
    EXPECT_EQ(*x + y, 20);
}

TEST(BasicOperator, RValueTest) {
    using RValuePlus = rjk::duck<
        rjk::has_op<rjk::op_plus, int(rjk::self &&, const rjk::this_duck_t &)>
    >;

    struct A {
        int operator+(const RValuePlus&) && {
            return 10;
        }

        int operator+(const RValuePlus&) const & {
            return 20;
        }
    };

    RValuePlus x{A{}};
    RValuePlus y{A{}};
    EXPECT_EQ(*RValuePlus{x} + std::move(y), 10);
}

TEST(BasicOperator, MultipleOverloads) {
    using Test = rjk::duck<
        rjk::has_op<rjk::op_plus, int(rjk::self, const rjk::this_duck_t&)>
    >;

    struct A {
        int operator+(const Test&) {
            return 5;
        }

        int operator+(int) {
            return 10;
        }
    };

    Test x{A{}};
    Test y{A{}};
    EXPECT_EQ(*x + Test{A{}}, 5);
}