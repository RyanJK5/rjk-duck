// clang-format off
#include "rjk/duck.hpp"

#include <gtest/gtest.h>

namespace rjk_test {

// ============================================================================
// Shared traits
// ============================================================================

struct [[=rjk::trait]] Nameable {
    std::string name() const;
};

struct [[=rjk::trait]] Sizeable {
    std::size_t size() const;
};

struct [[=rjk::trait]] Clearable {
    void clear();
    bool empty() const;
};

struct [[=rjk::trait]] TraitA {
    void mutate_a();
    int  query_a() const;
};

struct [[=rjk::trait]] TraitB {
    void mutate_b();
    int  query_b() const;
};

struct [[=rjk::trait]] ReadWrite {
    int  read()           const;
    int  transform(int)  const;
    void set(int);
    void zero();
};

// ============================================================================
// Basic trait composition: duck<Trait1, Trait2, Trait3>
// ============================================================================

using ComposedDuck = rjk::duck<Nameable, Sizeable, Clearable>;

// All methods from all three traits are accessible.
static_assert(requires(ComposedDuck& d) { d.name(); });
static_assert(requires(ComposedDuck& d) { d.size(); });
static_assert(requires(ComposedDuck& d) { d.clear(); });
static_assert(requires(ComposedDuck& d) { d.empty(); });

TEST(MultipleTraits, BasicComposition) {
    struct Container {
        std::string label;
        std::size_t count;
        bool        cleared = false;

        std::string name()  const { return label; }
        std::size_t size()  const { return count; }
        bool        empty() const { return count == 0 || cleared; }
        void        clear()       { cleared = true; }
    };

    ComposedDuck x{Container{"box", 3}};

    EXPECT_EQ(x.name(), "box");
    EXPECT_EQ(x.size(), 3u);
    EXPECT_FALSE(x.empty());

    x.clear();
    EXPECT_TRUE(x.empty());
}

TEST(MultipleTraits, BasicCompositionSwapType) {
    // Verify that swapping the underlying type works when multiple traits
    // are composed.
    struct TypeA {
        std::string name()  const { return "A"; }
        std::size_t size()  const { return 1; }
        bool        empty() const { return false; }
        void        clear()       {}
    };

    struct TypeB {
        std::string name()  const { return "B"; }
        std::size_t size()  const { return 2; }
        bool        empty() const { return true; }
        void        clear()       {}
    };

    ComposedDuck x{TypeA{}};
    EXPECT_EQ(x.name(), "A");
    EXPECT_EQ(x.size(), 1u);

    x = TypeB{};
    EXPECT_EQ(x.name(), "B");
    EXPECT_EQ(x.size(), 2u);
    EXPECT_TRUE(x.empty());
}

// ============================================================================
// Overlapping method name, different cvref qualifications across traits
// ============================================================================

struct [[=rjk::trait]] MutableValue {
    int& value();
};

struct [[=rjk::trait]] ConstValue {
    const int& value() const;
};

struct [[=rjk::trait]] RefQualifiedGet {
    int read() &;
    int read() &&;
    int read() const &;
};

// Both overloads of value() are present.
using ValueDuck = rjk::duck<MutableValue, ConstValue>;

TEST(MultipleTraits, OverlappingNameDifferentConst) {
    struct Box {
        int v;
        int&       value()       { return v; }
        const int& value() const { return v; }
    };

    ValueDuck x{Box{10}};
    const ValueDuck cx{Box{20}};

    x.value() = 99;
    EXPECT_EQ(rjk::get<Box>(x).v, 99);
    EXPECT_EQ(cx.value(), 20);
}

TEST(MultipleTraits, OverlappingNameRefQualifiers) {
    using MyDuck = rjk::duck<RefQualifiedGet>;

    struct Source {
        int v;
        int read() &       { return v + 1; }
        int read() &&      { return v + 2; }
        int read() const & { return v + 3; }
    };

    MyDuck x{Source{10}};
    const MyDuck cx{Source{10}};

    EXPECT_EQ(x.read(),            11); // lvalue ref overload
    EXPECT_EQ(std::move(x).read(), 12); // rvalue ref overload
    EXPECT_EQ(cx.read(),           13); // const lvalue ref overload
}

TEST(MultipleTraits, OverlappingNameAcrossTraits) {
    // Both traits contribute overloads to the same method name.
    // The duck should expose a unified overload set.
    struct [[=rjk::trait]] GetInt   { int  read(int) const; };
    struct [[=rjk::trait]] GetFloat { float read(float) const; };

    using MultiGetDuck = rjk::duck<GetInt, GetFloat>;

    struct Source {
        int   read(int) const { return 1; }
        float read(float) const { return 1.5f; }
    };

    MultiGetDuck x{Source{}};
    EXPECT_EQ(x.read(5),   1);
    EXPECT_EQ(x.read(10.f), 1.5f);
}

// ============================================================================
// duck<const Trait> -- only const members are brought in
// ============================================================================

using ReadOnlyDuck = rjk::duck<const ReadWrite>;

// Const methods are accessible.
static_assert(requires(ReadOnlyDuck& d) { d.read(); });
static_assert(requires(ReadOnlyDuck& d) { d.transform(0); });

// Non-const methods are not accessible.
template <typename T>
concept HasSet = requires(T t) { t.set(0); };

template <typename T>
concept HasReset = requires(T t) { t.zero(); };

static_assert(!HasSet<ReadOnlyDuck>);
static_assert(!HasReset<ReadOnlyDuck>);

TEST(MultipleTraits, ConstTrait) {
    struct Counter {
        int value = 0;
        int  read()            const { return value; }
        int  transform(int x) const { return value + x; }
        void set(int v)             { value = v; }
        void zero()                { value = 0; }
    };

    ReadOnlyDuck x{Counter{42}};

    EXPECT_EQ(x.read(), 42);
    EXPECT_EQ(x.transform(8), 50);
}

TEST(MultipleTraits, ConstTraitVsFullTrait) {
    // duck<Trait> exposes all methods; duck<const Trait> exposes only const ones.
    using FullDuck     = rjk::duck<ReadWrite>;
    using ReadOnlyDuck = rjk::duck<const ReadWrite>;

    static_assert(HasSet<FullDuck>);
    static_assert(HasReset<FullDuck>);
    static_assert(!HasSet<ReadOnlyDuck>);
    static_assert(!HasReset<ReadOnlyDuck>);
}

// ============================================================================
// duck<Trait1, const Trait2> -- mutable w.r.t. Trait1, const w.r.t. Trait2
// ============================================================================

using MixedDuck = rjk::duck<TraitA, const TraitB>;

// TraitA: both mutable and const methods are accessible.
static_assert(requires(MixedDuck& d) { d.mutate_a(); });
static_assert(requires(MixedDuck& d) { d.query_a(); });

// const TraitB: only const method is accessible, mutable is filtered out.
static_assert(requires(MixedDuck& d)  { d.query_b(); });

template <typename T>
concept CanMutateB = requires(T t) { t.mutate_b(); };
static_assert(!CanMutateB<MixedDuck>);

// For comparison: duck<TraitA, TraitB> exposes mutate_b.
static_assert(requires(rjk::duck<TraitA, TraitB>& d) { d.mutate_b(); });

TEST(MultipleTraits, MixedConstTrait) {
    struct Widget {
        int a = 0;
        int b = 100;

        void mutate_a()      { a += 10; }
        int  query_a() const { return a; }
        void mutate_b()      { b += 10; }
        int  query_b() const { return b; }
    };

    MixedDuck x{Widget{}};

    x.mutate_a();
    EXPECT_EQ(x.query_a(), 10);

    // b is unaffected since mutate_b is inaccessible.
    EXPECT_EQ(x.query_b(), 100);
}

TEST(MultipleTraits, MixedConstTraitBothConstMethodsAccessible) {
    // Both const methods from Trait1 and const Trait2 should be accessible
    // even on a const duck.
    struct Widget {
        int  query_a() const { return 1; }
        void mutate_a()      {}
        int  query_b() const { return 2; }
        void mutate_b()      {}
    };

    const MixedDuck x{Widget{}};

    EXPECT_EQ(x.query_a(), 1);
    EXPECT_EQ(x.query_b(), 2);
}

TEST(MultipleTraits, MixedConstOverloads) {
    struct [[=rjk::trait]] MixedConst {
        int operator()();
        int operator()() const;
    };

    struct Both {
        int operator()() { return 10; }

        int operator()() const { return 15; }
    };

    struct OnlyOne {
        int operator()() const { return 12; }
    };

    rjk::duck<MixedConst> mut{Both{}};
    EXPECT_EQ(mut(), 10);
    // mut = OnlyOne{}; ERROR

    const rjk::duck<MixedConst> const_mut{Both{}};
    EXPECT_EQ(const_mut(), 15);
    // const rjk::duck<MixedConst> const_mut{OnlyOne{}}; ERROR

    rjk::duck<const MixedConst> mut_const{Both{}};
    EXPECT_EQ(mut_const(), 15);
    mut_const = OnlyOne{};
    EXPECT_EQ(mut_const(), 12);
}

TEST(MultipleTraits, InheritedTraits) {
    struct [[=rjk::trait]] TraitBase1 {
        void foo();
    };

    using TraitBase2 = rjk::policy<rjk::has_fn<"doSmth", bool(int)>>;

    struct [[=rjk::trait]] TraitDerived : TraitBase1, TraitBase2, rjk::copyable {
        int bar();
    };

    struct A {
        int x = 5;
        void foo() { x = 10; }
        int bar() { return x; }
        bool doSmth(int y) { return y > 15; }
    };

    rjk::duck<TraitDerived> d{A{}};

    d.foo();
    EXPECT_EQ(d.bar(), 10);
    EXPECT_TRUE(d.doSmth(20));

    auto d2 = d;
    EXPECT_EQ(d2.bar(), 10);
}

TEST(MultipleTraits, ConstPolicy) {
    using MyPolicy = rjk::policy<
        rjk::has_fn<"foo", int()>,
        rjk::has_fn<"foo", int() const>
    >;

    struct TestStruct {
        int foo() { return 5; }
        int foo() const { return 10; }
    };

    rjk::duck<MyPolicy> d{TestStruct{}};
    EXPECT_EQ(d.foo(), 5);
    rjk::duck_view<const MyPolicy> view{d};
    EXPECT_EQ(view.foo(), 10);
}
} // namespace rjk_test