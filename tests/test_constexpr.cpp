// Constant-evaluated tests for construction and lifetime operations only.
//
// Calling a trait member on a duck, duck_view or duck_ptr can't be constant-
// evaluated: dispatch reaches the duck from its vtable_function member via
// trace_to_duck, which casts through void* to the enclosing wrapper. A
// static_cast from void* in a constant expression requires the pointer to
// point to an object of the target type (P2738), and the member is not a
// Wrapper, so a conforming compiler must reject it. GCC currently accepts it,
// so these tests must not rely on that. No check below calls a trait member
// (including operators) through a duck. Results are observed with
// rjk::get / rjk::get_if, rjk::is_valueless and destruction counters instead.
//
// During constant evaluation duck disables its small buffer optimization and
// always uses the heap (see `if !consteval` in detail/storage.hpp). The
// Small / Big / ThrowingMove distinction therefore makes no difference at
// compile time; it is kept so these checks still cover each storage category
// if constant evaluation ever starts using the small buffer.
//
// Several checks assert that a moved-from duck is rjk::is_valueless. The docs
// don't state this, but tests/allocators/valueless.cpp tests the same
// behaviour; these checks pin it on purpose.

#include "rjk/duck.hpp"

#include <array>
#include <concepts>
#include <memory>
#include <ranges>
#include <utility>

#include <gtest/gtest.h>

namespace rjk_test::constexpr_tests {
namespace {

struct Valued {
    auto value() const -> int;
};

struct Settable {
    auto value() const -> int;
    auto set(int v) -> void;
};

struct Small {
    int v{};
    constexpr auto value() const -> int { return v; }
    constexpr auto set(int x) -> void { v = x; }
};

// Larger than the default SBO buffer
struct Big {
    std::array<int, 16> data{};
    constexpr Big() = default;
    constexpr explicit Big(int v) { data[0] = v; }
    constexpr auto value() const -> int { return data[0]; }
    constexpr auto set(int x) -> void { data[0] = x; }
};

// Not nothrow-movable, so never SBO-resident at runtime either
struct ThrowingMove {
    int v{};
    constexpr ThrowingMove(int x) : v(x) {}
    constexpr ThrowingMove(const ThrowingMove& o) : v(o.v) {}
    constexpr ThrowingMove(ThrowingMove&& o) noexcept(false) : v(o.v) {}
    constexpr auto operator=(const ThrowingMove&) -> ThrowingMove& = default;
    constexpr auto value() const -> int { return v; }
    constexpr auto set(int x) -> void { v = x; }
};

// Records destruction through a pointer so lifetimes can be checked
struct Tracked {
    int* destroyed{};
    int v{};
    constexpr Tracked(int* d, int x) : destroyed(d), v(x) {}
    constexpr Tracked(Tracked&& o) noexcept : destroyed(std::exchange(o.destroyed, nullptr)), v(o.v) {}
    constexpr Tracked(const Tracked& o) : destroyed(o.destroyed), v(o.v) {}
    constexpr ~Tracked() { if (destroyed) { ++*destroyed; } }
    constexpr auto value() const -> int { return v; }
};

struct WithList {
    int sum{};
    constexpr WithList(std::initializer_list<int> il, int extra) : sum(extra) {
        for (int i : il) { sum += i; }
    }
    constexpr auto value() const -> int { return sum; }
};

using ValDuck = rjk::duck<Valued>;
using CopyValDuck = rjk::duck<Valued, rjk::copyable>;
using SetDuck = rjk::duck<Settable>;
using CopySetDuck = rjk::duck<Settable, rjk::copyable>;

// Helpers to read the held object without dispatching through the vtable
template <typename T, typename D>
constexpr auto held(const D& d) -> const T* { return rjk::get_if<T>(&d); }

constexpr auto small_v(const auto& d) -> int { return rjk::get<Small>(d).v; }

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

static_assert([] {
    ValDuck d{Small{5}};
    return small_v(d) == 5 && held<Big>(d) == nullptr && !rjk::is_valueless(d);
}());

static_assert([] {
    ValDuck d{Big{7}};
    return rjk::get<Big>(d).data[0] == 7;
}());

static_assert([] {
    ValDuck d{ThrowingMove{9}};
    return rjk::get<ThrowingMove>(d).v == 9;
}());

static_assert([] {
    const Small s{3};
    ValDuck d{s}; // copy from lvalue
    return small_v(d) == 3;
}());

static_assert([] {
    ValDuck d{std::in_place_type<Big>, 11};
    return rjk::get<Big>(d).data[0] == 11;
}());

static_assert([] {
    ValDuck d{std::in_place_type<WithList>, {1, 2, 3}, 4};
    return rjk::get<WithList>(d).sum == 10;
}());

static_assert([] {
    const ValDuck d{Small{8}};
    return held<Small>(d) != nullptr && held<Small>(d)->v == 8;
}());

static_assert([] {
    rjk::duck<const Settable> d{Small{6}};
    return small_v(d) == 6;
}());

static_assert([] {
    int destroyed = 0;
    {
        ValDuck d{Tracked{&destroyed, 1}};
    }
    return destroyed == 1;
}());

// ---------------------------------------------------------------------------
// Assignment from objects
// ---------------------------------------------------------------------------

static_assert([] {
    ValDuck d{Small{1}};
    d = Big{2};
    if (rjk::get<Big>(d).data[0] != 2 || held<Small>(d) != nullptr) { return false; }
    d = Small{3};
    if (small_v(d) != 3) { return false; }
    d = ThrowingMove{4};
    return rjk::get<ThrowingMove>(d).v == 4;
}());

static_assert([] {
    int destroyed = 0;
    {
        ValDuck d{Tracked{&destroyed, 1}};
        d = Small{2}; // destroys the held Tracked
        if (destroyed != 1 || small_v(d) != 2) { return false; }
    }
    return destroyed == 1;
}());

// ---------------------------------------------------------------------------
// Copy
// ---------------------------------------------------------------------------

static_assert([] {
    CopyValDuck a{Small{5}};
    CopyValDuck b{a};
    return small_v(a) == 5 && small_v(b) == 5 && held<Small>(a) != held<Small>(b);
}());

static_assert([] {
    CopySetDuck a{Big{5}};
    CopySetDuck b{a};
    rjk::get<Big>(b).data[0] = 6;
    return rjk::get<Big>(a).data[0] == 5 && rjk::get<Big>(b).data[0] == 6; // independent copies
}());

static_assert([] {
    CopySetDuck a{Small{1}};
    CopySetDuck b{Big{2}};
    b = a;
    rjk::get<Small>(a).v = 10;
    return small_v(a) == 10 && small_v(b) == 1;
}());

static_assert([] {
    CopyValDuck a{Small{1}};
    a = static_cast<const CopyValDuck&>(a); // self copy-assign
    return small_v(a) == 1;
}());

static_assert([] {
    int destroyed = 0;
    {
        CopyValDuck a{Tracked{&destroyed, 1}};
        CopyValDuck b{a};
        CopyValDuck c{Small{0}};
        c = b;
    }
    return destroyed == 3;
}());

// ---------------------------------------------------------------------------
// Move
// ---------------------------------------------------------------------------

static_assert([] {
    ValDuck a{Small{5}};
    ValDuck b{std::move(a)};
    return small_v(b) == 5 && rjk::is_valueless(a);
}());

static_assert([] {
    ValDuck a{Big{5}};
    ValDuck b{Small{1}};
    b = std::move(a);
    return rjk::get<Big>(b).data[0] == 5 && rjk::is_valueless(a);
}());

static_assert([] {
    // moved-from duck can be reassigned
    ValDuck a{Small{5}};
    ValDuck b{std::move(a)};
    a = Small{6};
    return small_v(a) == 6 && small_v(b) == 5;
}());

static_assert([] {
    ValDuck a{Small{5}};
    ValDuck& self = a;
    a = std::move(self); // self move-assign
    return small_v(a) == 5;
}());

static_assert([] {
    int destroyed = 0;
    {
        ValDuck a{Tracked{&destroyed, 1}};
        ValDuck b{std::move(a)};
        ValDuck c{Small{0}};
        c = std::move(b);
        if (destroyed != 0) { return false; }
    }
    return destroyed == 1;
}());

// ---------------------------------------------------------------------------
// emplace
// ---------------------------------------------------------------------------

static_assert([] {
    ValDuck d{Small{1}};
    Big& b = rjk::emplace<Big>(d, 9);
    return &b == held<Big>(d) && b.data[0] == 9;
}());

static_assert([] {
    ValDuck d{Big{1}};
    rjk::emplace<Small>(d, 3);
    return small_v(d) == 3;
}());

static_assert([] {
    ValDuck d{Small{1}};
    rjk::emplace<WithList>(d, {1, 2}, 3);
    return rjk::get<WithList>(d).sum == 6;
}());

static_assert([] {
    ValDuck d{Small{1}};
    rjk::emplace<ThrowingMove>(d, 4);
    return rjk::get<ThrowingMove>(d).v == 4;
}());

static_assert([] {
    int destroyed = 0;
    {
        ValDuck d{Tracked{&destroyed, 1}};
        rjk::emplace<Tracked>(d, &destroyed, 2);
        if (destroyed != 1 || rjk::get<Tracked>(d).v != 2) { return false; }
    }
    return destroyed == 2;
}());

// ---------------------------------------------------------------------------
// swap
// ---------------------------------------------------------------------------

static_assert([] {
    ValDuck a{Small{1}};
    ValDuck b{Big{2}};
    swap(a, b);
    return rjk::get<Big>(a).data[0] == 2 && small_v(b) == 1;
}());

static_assert([] {
    ValDuck a{Small{1}};
    ValDuck b{Small{2}};
    std::ranges::swap(a, b);
    return small_v(a) == 2 && small_v(b) == 1;
}());

static_assert([] {
    ValDuck a{Small{1}};
    swap(a, a);
    return small_v(a) == 1;
}());

// ---------------------------------------------------------------------------
// Narrowing conversions (docs/ducks/03_trait_narrowing.md)
// ---------------------------------------------------------------------------

struct TraitA {
    auto a() const -> int;
};

struct TraitB {
    auto b() -> int;
};

struct AB {
    int x{};
    constexpr auto a() const -> int { return x; }
    constexpr auto b() -> int { return ++x; }
};

struct BigAB {
    std::array<int, 16> pad{};
    int x{};
    constexpr auto a() const -> int { return x; }
    constexpr auto b() -> int { return ++x; }
};

static_assert([] {
    rjk::duck<TraitA, TraitB> d{AB{1}};
    rjk::duck<TraitA> n = rjk::make_narrowed<TraitA>(std::move(d));
    return rjk::get<AB>(n).x == 1 && rjk::is_valueless(d);
}());

struct CopyableB : TraitB, rjk::copyable {};

static_assert([] {
    // copy-narrowing from an lvalue
    rjk::duck<TraitA, CopyableB> d{BigAB{{}, 1}};
    rjk::duck<CopyableB> n = rjk::make_narrowed<CopyableB>(d);
    rjk::get<BigAB>(n).x = 2;
    return rjk::get<BigAB>(d).x == 1 && rjk::get<BigAB>(n).x == 2;
}());

static_assert([] {
    rjk::duck<TraitA, TraitB> d{AB{1}};
    rjk::duck<const TraitA, const TraitB> c = rjk::make_narrowed<const TraitA, const TraitB>(std::move(d));
    return rjk::get<AB>(c).x == 1;
}());

static_assert([] {
    // permutation is implicit
    rjk::duck<TraitA, TraitB> d{AB{2}};
    rjk::duck<TraitB, TraitA> p = std::move(d);
    return rjk::get<AB>(p).x == 2 && rjk::is_valueless(d);
}());

// ---------------------------------------------------------------------------
// duck_view (construction and rebinding only)
// ---------------------------------------------------------------------------

static_assert([] {
    Small s{3};
    rjk::duck_view<Settable> v{s};
    return rjk::get_if<Small>(&v) == &s && rjk::get_if<Big>(&v) == nullptr;
}());

static_assert([] {
    const Small s{3};
    rjk::duck_view<const Settable> v{s};
    return rjk::get_if<Small>(&v) == &s;
}());

static_assert([] {
    SetDuck d{Big{1}};
    rjk::duck_view<Settable> v{d};
    return rjk::get_if<Big>(&v) == held<Big>(d);
}());

static_assert([] {
    const SetDuck d{Small{2}};
    rjk::duck_view<const Settable> v{d};
    return rjk::get_if<Small>(&v) == held<Small>(d);
}());

static_assert([] {
    rjk::duck<TraitA, TraitB> d{AB{1}};
    rjk::duck_view<TraitA, TraitB> v{d};
    rjk::duck_view<TraitB> vb{v}; // view -> narrower view
    rjk::duck_view<const TraitA, const TraitB> vc{v};
    return rjk::get_if<AB>(&vb) == held<AB>(d) && rjk::get_if<AB>(&vc) == held<AB>(d);
}());

static_assert([] {
    Small a{1};
    Small b{2};
    rjk::duck_view<Settable> v{a};
    rjk::duck_view<Settable> w{b};
    v = w; // views rebind on assignment
    return rjk::get_if<Small>(&v) == &b && a.v == 1;
}());

#if 0
// FINDING-6 (PR #69, unclear whether intended): there is a public deduction guide
// `duck(duck_view<Traits...>) -> duck<Traits...>`, but the duck constructors
// taking a duck_view are private, and for an identical trait list
// (is_permutation) they are excluded anyway, so a duck cannot be constructed
// from a duck_view at all ("no matching function for call to duck(...)").
static_assert([] {
    // duck constructed from a duck_view copies the viewed object
    Big b{5};
    rjk::duck_view<Settable, rjk::copyable> v{b};
    rjk::duck<Settable, rjk::copyable> d{v};
    rjk::get<Big>(d).data[0] = 6;
    return b.data[0] == 5 && rjk::get<Big>(d).data[0] == 6;
}());
#endif

// ---------------------------------------------------------------------------
// duck_ptr (construction and reset only)
// ---------------------------------------------------------------------------

static_assert([] {
    Small s{3};
    rjk::duck_ptr<Settable> p{s};
    if (!p.has_value()) { return false; }
    p.reset();
    return !p && !p.has_value();
}());

static_assert([] {
    ValDuck d{Small{4}};
    rjk::duck_ptr p{d};
    return static_cast<bool>(p);
}());

// ---------------------------------------------------------------------------
// Allocator-extended constructors
// ---------------------------------------------------------------------------

static_assert([] {
    std::allocator<std::byte> alloc;
    ValDuck d{std::allocator_arg, alloc, Small{4}};
    ValDuck e{std::allocator_arg, alloc, std::move(d)};
    return small_v(e) == 4 && rjk::is_valueless(d);
}());

static_assert([] {
    std::allocator<std::byte> alloc;
    CopyValDuck d{std::allocator_arg, alloc, std::in_place_type<Big>, 3};
    CopyValDuck e{std::allocator_arg, alloc, d};
    return rjk::get<Big>(e).data[0] == 3 && rjk::get<Big>(d).data[0] == 3;
}());

// The same operations at runtime, plus member calls (which are only valid
// outside constant evaluation).
TEST(Constexpr, SameCodeAtRuntime) {
    ValDuck d{Small{1}};
    d = Big{2};
    EXPECT_EQ(d.value(), 2);
    ValDuck e{std::move(d)};
    EXPECT_EQ(e.value(), 2);
    EXPECT_TRUE(rjk::is_valueless(d));
    CopySetDuck a{Small{1}};
    CopySetDuck b{a};
    b.set(3);
    EXPECT_EQ(a.value(), 1);
    EXPECT_EQ(b.value(), 3);
}

}  // namespace
}  // namespace rjk_test::constexpr_tests
