// Constant-evaluated tests. During constant evaluation duck disables its
// small buffer optimization and always uses the heap (see `if !consteval` in
// detail/storage.hpp), so these exercise a separate code path from the
// runtime tests. Every check is a static_assert over an immediately-invoked
// lambda; the TEST at the bottom only re-runs a subset at runtime to make sure
// the same code also works when not constant-evaluated.

#include "rjk/duck.hpp"

#include <array>
#include <memory>
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

// ---------------------------------------------------------------------------
// Construction and calls
// ---------------------------------------------------------------------------

static_assert([] {
    ValDuck d{Small{5}};
    return d.value() == 5;
}());

static_assert([] {
    ValDuck d{Big{7}};
    return d.value() == 7;
}());

static_assert([] {
    ValDuck d{ThrowingMove{9}};
    return d.value() == 9;
}());

static_assert([] {
    const Small s{3};
    ValDuck d{s}; // copy from lvalue
    return d.value() == 3;
}());

static_assert([] {
    ValDuck d{std::in_place_type<Big>, 11};
    return d.value() == 11;
}());

static_assert([] {
    ValDuck d{std::in_place_type<WithList>, {1, 2, 3}, 4};
    return d.value() == 10;
}());

static_assert([] {
    SetDuck d{Small{1}};
    d.set(42);
    return d.value() == 42;
}());

static_assert([] {
    const ValDuck d{Small{8}};
    return d.value() == 8; // const member through const duck
}());

static_assert([] {
    rjk::duck<const Settable> d{Small{6}};
    return d.value() == 6;
}());

// ---------------------------------------------------------------------------
// Assignment from objects
// ---------------------------------------------------------------------------

static_assert([] {
    ValDuck d{Small{1}};
    d = Big{2};
    if (d.value() != 2) { return false; }
    d = Small{3};
    if (d.value() != 3) { return false; }
    d = ThrowingMove{4};
    return d.value() == 4;
}());

static_assert([] {
    int destroyed = 0;
    {
        ValDuck d{Tracked{&destroyed, 1}};
        d = Small{2}; // destroys the held Tracked
        if (destroyed != 1) { return false; }
    }
    return destroyed == 1;
}());

static_assert([] {
    int destroyed = 0;
    {
        ValDuck d{Tracked{&destroyed, 1}};
    }
    return destroyed == 1;
}());

// ---------------------------------------------------------------------------
// Copy
// ---------------------------------------------------------------------------

static_assert([] {
    CopyValDuck a{Small{5}};
    CopyValDuck b{a};
    return a.value() == 5 && b.value() == 5;
}());

static_assert([] {
    CopySetDuck a{Big{5}};
    CopySetDuck b{a};
    b.set(6);
    return a.value() == 5 && b.value() == 6; // independent copies
}());

static_assert([] {
    CopySetDuck a{Small{1}};
    CopySetDuck b{Big{2}};
    b = a;
    a.set(10);
    return a.value() == 10 && b.value() == 1;
}());

static_assert([] {
    CopyValDuck a{Small{1}};
    a = static_cast<const CopyValDuck&>(a); // self copy-assign
    return a.value() == 1;
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
    return b.value() == 5 && rjk::is_valueless(a);
}());

static_assert([] {
    ValDuck a{Big{5}};
    ValDuck b{Small{1}};
    b = std::move(a);
    return b.value() == 5 && rjk::is_valueless(a);
}());

static_assert([] {
    // moved-from duck can be reassigned
    ValDuck a{Small{5}};
    ValDuck b{std::move(a)};
    a = Small{6};
    return a.value() == 6 && b.value() == 5;
}());

static_assert([] {
    ValDuck a{Small{5}};
    ValDuck& self = a;
    a = std::move(self); // self move-assign
    return a.value() == 5;
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
    return d.value() == 9 && b.value() == 9;
}());

static_assert([] {
    ValDuck d{Big{1}};
    rjk::emplace<Small>(d, 3);
    return d.value() == 3;
}());

static_assert([] {
    ValDuck d{Small{1}};
    rjk::emplace<WithList>(d, {1, 2}, 3);
    return d.value() == 6;
}());

static_assert([] {
    ValDuck d{Small{1}};
    rjk::emplace<ThrowingMove>(d, 4);
    return d.value() == 4;
}());

static_assert([] {
    int destroyed = 0;
    {
        ValDuck d{Tracked{&destroyed, 1}};
        rjk::emplace<Tracked>(d, &destroyed, 2);
        if (destroyed != 1 || d.value() != 2) { return false; }
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
    return a.value() == 2 && b.value() == 1;
}());

static_assert([] {
    ValDuck a{Small{1}};
    ValDuck b{Small{2}};
    std::ranges::swap(a, b);
    return a.value() == 2 && b.value() == 1;
}());

static_assert([] {
    ValDuck a{Small{1}};
    swap(a, a);
    return a.value() == 1;
}());

// ---------------------------------------------------------------------------
// get / get_if
// ---------------------------------------------------------------------------

static_assert([] {
    ValDuck d{Big{4}};
    if (rjk::get<Big>(d).value() != 4) { return false; }
    if (rjk::get_if<Small>(&d) != nullptr) { return false; }
    rjk::get<Big>(d).set(5);
    return rjk::get_if<Big>(&d)->value() == 5;
}());

// ---------------------------------------------------------------------------
// Narrowing (docs/ducks/03_trait_narrowing.md)
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
    return n.a() == 1 && rjk::is_valueless(d);
}());

struct CopyableB : TraitB, rjk::copyable {};

static_assert([] {
    // copy-narrowing from an lvalue
    rjk::duck<TraitA, CopyableB> d{BigAB{{}, 1}};
    rjk::duck<CopyableB> n = rjk::make_narrowed<CopyableB>(d);
    n.b();
    return d.a() == 1 && n.b() == 3;
}());

static_assert([] {
    rjk::duck<TraitA, TraitB> d{AB{1}};
    rjk::duck<const TraitA, const TraitB> c = rjk::make_narrowed<const TraitA, const TraitB>(std::move(d));
    return c.a() == 1;
}());

static_assert([] {
    // permutation is implicit
    rjk::duck<TraitA, TraitB> d{AB{2}};
    rjk::duck<TraitB, TraitA> p = std::move(d);
    p.b();
    return p.a() == 3;
}());

// ---------------------------------------------------------------------------
// duck_view
// ---------------------------------------------------------------------------

static_assert([] {
    Small s{3};
    rjk::duck_view<Settable> v{s};
    v.set(4);
    return s.v == 4 && v.value() == 4;
}());

static_assert([] {
    const Small s{3};
    rjk::duck_view<const Settable> v{s};
    return v.value() == 3;
}());

static_assert([] {
    SetDuck d{Big{1}};
    rjk::duck_view<Settable> v{d};
    v.set(7);
    return d.value() == 7;
}());

static_assert([] {
    const SetDuck d{Small{2}};
    rjk::duck_view<const Settable> v{d};
    return v.value() == 2;
}());

static_assert([] {
    rjk::duck<TraitA, TraitB> d{AB{1}};
    rjk::duck_view<TraitB> vb{d};
    rjk::duck_view<const TraitA> va{d};
    vb.b();
    return va.a() == 2;
}());

static_assert([] {
    rjk::duck<TraitA, TraitB> d{AB{1}};
    rjk::duck_view<TraitA, TraitB> v{d};
    rjk::duck_view<TraitB> vb{v}; // view -> narrower view
    rjk::duck_view<const TraitA, const TraitB> vc{v};
    vb.b();
    return vc.a() == 2;
}());

static_assert([] {
    Small a{1};
    Small b{2};
    rjk::duck_view<Settable> v{a};
    rjk::duck_view<Settable> w{b};
    v = w; // views rebind on assignment
    v.set(5);
    return a.v == 1 && b.v == 5;
}());

static_assert([] {
    Small s{3};
    rjk::duck_view<Settable> v{s};
    return rjk::get_if<Small>(&v) == &s && rjk::get_if<Big>(&v) == nullptr;
}());

#if 0
// FINDING (unclear whether intended): there is a public deduction guide
// `duck(duck_view<Traits...>) -> duck<Traits...>`, but the duck constructors
// taking a duck_view are private, and for an identical trait list
// (is_permutation) they are excluded anyway, so a duck cannot be constructed
// from a duck_view at all ("no matching function for call to duck(...)").
static_assert([] {
    // duck constructed from a duck_view copies the viewed object
    Big b{5};
    rjk::duck_view<Settable, rjk::copyable> v{b};
    rjk::duck<Settable, rjk::copyable> d{v};
    d.set(6);
    return b.value() == 5 && d.value() == 6;
}());
#endif

// ---------------------------------------------------------------------------
// duck_ptr
// ---------------------------------------------------------------------------

static_assert([] {
    Small s{3};
    rjk::duck_ptr<Settable> p{s};
    if (!p.has_value()) { return false; }
    p->set(4);
    p.reset();
    return !p && s.v == 4;
}());

static_assert([] {
    ValDuck d{Small{4}};
    rjk::duck_ptr p{d};
    return p->value() == 4 && (*p).value() == 4;
}());

// ---------------------------------------------------------------------------
// Allocator-extended constructors
// ---------------------------------------------------------------------------

static_assert([] {
    std::allocator<std::byte> alloc;
    ValDuck d{std::allocator_arg, alloc, Small{4}};
    ValDuck e{std::allocator_arg, alloc, std::move(d)};
    return e.value() == 4 && rjk::is_valueless(d);
}());

static_assert([] {
    std::allocator<std::byte> alloc;
    CopyValDuck d{std::allocator_arg, alloc, std::in_place_type<Big>, 3};
    CopyValDuck e{std::allocator_arg, alloc, d};
    return e.value() == 3 && d.value() == 3;
}());

// ---------------------------------------------------------------------------
// Operators
// ---------------------------------------------------------------------------

struct Callable {
    auto operator()(int) const -> int;
};

struct Addable {
    auto operator+(int) const -> int;
};

static_assert([] {
    rjk::duck<Callable> d{[](int x) { return x * 2; }};
    return d(21) == 42;
}());

static_assert([] {
    auto l = [](int x) { return x + 1; };
    rjk::duck_view<const Callable> v{l};
    return v(1) == 2;
}());

static_assert([] {
    rjk::duck<Addable> d{5};
    return d + 3 == 8;
}());

// ---------------------------------------------------------------------------
// Duck covariance (docs/ducks/04_duck_covariance.md)
// ---------------------------------------------------------------------------

struct Clonable {
    auto value() const -> int;
    auto clone() const -> rjk::duck<Clonable>;
};

struct CloneMe {
    int v{};
    constexpr auto value() const -> int { return v; }
    constexpr auto clone() const -> CloneMe { return CloneMe{v + 1}; }
};

static_assert([] {
    rjk::duck<Clonable> d{CloneMe{1}};
    rjk::duck<Clonable> c = d.clone();
    return c.value() == 2 && c.clone().value() == 3;
}());

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
