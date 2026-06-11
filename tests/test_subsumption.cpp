#include <gtest/gtest.h>
#include <numeric>
#include <vector>

#include "duck.hpp"
#include "duck_view.hpp"

namespace rjk_test {

// ============================================================
// Trait Definitions & Mock Objects
// ============================================================

struct [[=rjk::trait]] Alpha { void setA(int v); int getA() const; };
struct [[=rjk::trait]] Beta  { void setB(int v); int getB() const; };
struct [[=rjk::trait]] Gamma { void setG(int v); int getG() const; };

struct ObjABG {
    int a = 0, b = 0, g = 0;
    void setA(int v) { a = v; } int getA() const { return a; }
    void setB(int v) { b = v; } int getB() const { return b; }
    void setG(int v) { g = v; } int getG() const { return g; }
};

// Free functions to verify implicit conversions
static int f_readA(rjk::duck_view<const Alpha> v) { return v.getA(); }

template <rjk::is_trait T> static int f_template_read(rjk::duck_view<const T> v);
template <> int f_template_read<Alpha>(rjk::duck_view<const Alpha> v) { return v.getA(); }
template <> int f_template_read<Beta>(rjk::duck_view<const Beta> v)   { return v.getB(); }

// ============================================================
// Rule 1 – Identity: D<Ts...> from D<Ts...>
// ============================================================
TEST(SubsumptionSuite, Rule1_Identity) {
    ObjABG obj{.a = 1, .b = 2};
    rjk::duck<Alpha, Beta> d{obj};

    // Copy-construction from matching duck
    rjk::duck_view<Alpha, Beta> view_from_duck{d};
    EXPECT_EQ(view_from_duck.getA(), 1);

    // Copy-construction/assignment from matching view (aliasing check)
    rjk::duck_view<Alpha, Beta> view_from_view{view_from_duck};
    view_from_view.setA(42);
    EXPECT_EQ(d.getA(), 42);
    EXPECT_EQ(view_from_duck.getA(), 42);
}

// ============================================================
// Rule 2 – All-const: D<const Ts...> from D<Ts...>
// ============================================================
TEST(SubsumSuite, Rule2_AllConst) {
    ObjABG obj{.a = 1, .b = 2};
    rjk::duck<Alpha, Beta> d{obj};
    rjk::duck_view<Alpha, Beta> wide_view{d};

    // Construction and implicit conversion from mutable/const anchors
    rjk::duck_view<const Alpha, const Beta> c_view{wide_view};
    EXPECT_EQ(c_view.getA(), 1);
    EXPECT_EQ(f_readA(d), 1); // Implicitly creates duck_view<const Alpha> from duck

    // Rebinding via assignment
    rjk::duck<Alpha, Beta> d2{ObjABG{.a = 9}};
    c_view = d2;
    EXPECT_EQ(c_view.getA(), 9);
}

// ============================================================
// Rule 3 – Single-trait extraction: D<T> from D<Ts...>
// ============================================================
TEST(SubsumptionSuite, Rule3_SingleTraitExtraction) {
    ObjABG obj{.a = 10, .b = 20, .g = 30};
    rjk::duck<Alpha, Beta, Gamma> d{obj};
    rjk::duck_view<Alpha, Beta, Gamma> wide{d};

    // Slice out the middle trait (Beta) from a 3-trait view and duck
    rjk::duck_view<Beta> narrow_from_view{wide};
    rjk::duck_view<Beta> narrow_from_duck{d};

    EXPECT_EQ(narrow_from_view.getB(), 20);
    EXPECT_EQ(narrow_from_duck.getB(), 20);

    // Mutation propagation & structural isolation
    narrow_from_view.setB(99);
    EXPECT_EQ(d.getB(), 99);
    EXPECT_EQ(wide.getA(), 10); // Siblings remain intact
}

// ============================================================
// Rule 4 – Single const-trait extraction: D<const T> from D<Ts...>
// ============================================================
TEST(SubsumptionSuite, Rule4_SingleConstTraitExtraction) {
    ObjABG obj{.a = 10, .b = 20};
    const rjk::duck<Alpha, Beta> const_d{obj};
    rjk::duck_view<const Alpha, const Beta> wide{const_d};

    // Extracting const views from mutable wide views and const containers
    rjk::duck_view<const Alpha> c_view_from_wide{wide};
    rjk::duck_view<const Beta> c_view_from_const_duck{const_d};

    EXPECT_EQ(c_view_from_wide.getA(), 10);
    EXPECT_EQ(c_view_from_const_duck.getB(), 20);
}

// ============================================================
// Complex Combinatorics, Lifecycles & Heterogeneous Containers
// ============================================================
TEST(SubsumptionSuite, ArchitecturalEdgeCases) {
    ObjABG obj1{.a = 1, .b = 2}, obj2{.a = 100, .b = 200};
    rjk::duck<Alpha, Beta> d1{obj1}, d2{obj2};

    // 1. Rebinding Correctness (Ensure the old target object is isolated after rebind)
    rjk::duck_view<Alpha> target_view{d1};
    target_view = d2;
    target_view.setA(999);
    EXPECT_EQ(d1.getA(), 1);    // Unchanged
    EXPECT_EQ(d2.getA(), 999);  // Mutated

    // 2. Transitivity (Multi-step subsumptions: Duck -> Wide View -> Narrow View -> Const Narrow View)
    rjk::duck_view<const Alpha> transitive_view{ rjk::duck_view<Alpha>{d2} };
    EXPECT_EQ(transitive_view.getA(), 999);

    // 3. Template Deduction with Concepts
    EXPECT_EQ(f_template_read<Alpha>(d2), 999);
    EXPECT_EQ(f_template_read<Beta>(d2), 200);

    // 4. Heterogeneous Pipeline (Using Rule 2, 3, and 4 variants in std containers)
    std::vector<rjk::duck_view<const Alpha>> heterogeneous_vec;
    heterogeneous_vec.emplace_back(obj1); // From concrete type
    heterogeneous_vec.emplace_back(d2);   // Subsumed from duck

    int total = std::accumulate(heterogeneous_vec.begin(), heterogeneous_vec.end(), 0,
        [](int acc, const auto& view) { return acc + view.getA(); });
    EXPECT_EQ(total, 1 + 999);
}

// ============================================================
// narrow_duck Facility
// ============================================================

struct [[=rjk::trait]] CopyableA : Alpha, rjk::copyable {};
struct [[=rjk::trait]] CopyableB : Beta, rjk::copyable {};
struct [[=rjk::trait]] CopyableG : Gamma, rjk::copyable {};

TEST(SubsumptionSuite, NarrowDuck_BasicFromDuck) {
    ObjABG obj{.a = 10, .b = 20, .g = 30};
    rjk::duck<CopyableA, CopyableB, CopyableG> wide{obj};

    auto a_duck = rjk::narrow_duck<CopyableA>(wide);
    auto b_duck = rjk::narrow_duck<CopyableB>(wide);
    auto g_duck = rjk::narrow_duck<CopyableG>(wide);

    EXPECT_EQ(a_duck.getA(), 10);
    EXPECT_EQ(b_duck.getB(), 20);
    EXPECT_EQ(g_duck.getG(), 30);

    a_duck.setA(50);
    b_duck.setB(50);
    g_duck.setG(50);
    EXPECT_EQ(wide.getA(), 10);
    EXPECT_EQ(wide.getB(), 20);
    EXPECT_EQ(wide.getG(), 30);
}

TEST(SubsumptionSuite, NarrowDuck_BasicFromView) {
    ObjABG obj{.a = 10, .b = 20, .g = 30};
    rjk::duck<CopyableA, Beta, Gamma> wide{obj};
    rjk::duck_view<CopyableA, Beta, Gamma> view{wide};

    auto a_duck = rjk::narrow_duck<CopyableA>(view);
    EXPECT_EQ(a_duck.getA(), 10);

    // Mutations to the narrow duck don't affect the view's underlying object
    a_duck.setA(77);
    EXPECT_EQ(wide.getA(), 10);
}

// ============================================================
// Constructing duck from duck_view
// ============================================================

TEST(SubsumptionSuite, DuckFromView_Identity) {
    ObjABG obj{.a = 3, .b = 6};
    rjk::duck<Alpha, Beta, rjk::copyable> orig{obj};
    rjk::duck_view<Alpha, Beta, rjk::copyable> view{orig};

    // duck constructed from a same-trait view owns a copy
    rjk::duck<Alpha, Beta, rjk::copyable> copied{view};
    EXPECT_EQ(copied.getA(), 3);

    copied.setA(33);
    EXPECT_EQ(orig.getA(), 3);  // view's underlying object unaffected
}

TEST(SubsumptionSuite, DuckFromView_ConstStrip) {
    ObjABG obj{.a = 4, .b = 8};
    rjk::duck<CopyableA, Beta> orig{obj};
    rjk::duck_view<const CopyableA, const Beta> cv{orig};

    rjk::duck d{cv};
    static_assert(std::same_as<decltype(d), rjk::duck<const CopyableA, const Beta>>);
    EXPECT_EQ(d.getA(), 4);
}

TEST(SubsumptionSuite, DuckFromView_MoveFromView) {
    ObjABG obj{.a = 6};
    rjk::duck<Alpha, rjk::copyable> orig{obj};
    rjk::duck_view<Alpha, rjk::copyable> view{orig};

    // Moving the view into a duck should be the same as copying
    rjk::duck<Alpha, rjk::copyable> from_moved_view{std::move(view)};
    EXPECT_EQ(from_moved_view.getA(), 6);
    from_moved_view.setA(60);
    EXPECT_EQ(orig.getA(), 6); // still isolated
}

// ============================================================
// Negative / compile-time assertions
// ============================================================

TEST(SubsumptionSuite, NarrowDuck_StaticAssertions) {
    // These should all be well-formed type relationships
    static_assert(std::same_as<
        decltype(rjk::narrow_duck<CopyableA>(std::declval<rjk::duck<CopyableA, CopyableB>&>())),
        rjk::duck<CopyableA>
    >);
    static_assert(std::same_as<
        decltype(rjk::narrow_duck<CopyableA>(std::declval<rjk::duck_view<CopyableA, CopyableB>>())),
        rjk::duck<CopyableA>
    >);

    // narrow_duck from a matching single-trait duck is a no-op copy
    static_assert(std::same_as<
        decltype(rjk::narrow_duck<CopyableA>(std::declval<rjk::duck<CopyableA>&>())),
        rjk::duck<CopyableA>
    >);
}

} // namespace rjk_test::subsumption