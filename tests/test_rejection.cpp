// Negative conformance tests: every check in this file asserts that something
// is *rejected* at compile time (or, for a handful of positive controls, that
// the closest accepted variant really is accepted, so the negative check is
// not vacuously true).

#include "rjk/duck.hpp"

#include <concepts>
#include <string>
#include <type_traits>

#include <gtest/gtest.h>

namespace rjk_test::rejection {
namespace {

// ---------------------------------------------------------------------------
// Missing members / wrong names / wrong arity
// ---------------------------------------------------------------------------

struct Fooable {
    auto foo() -> int;
};

struct Empty {};

struct HasFoo {
    auto foo() -> int { return 1; }
};

struct HasBar {
    auto bar() -> int { return 1; }
};

struct FooTakesInt {
    auto foo(int) -> int { return 1; }
};

struct FooTakesTwoInts {
    auto foo(int, int) -> int { return 1; }
};

struct StaticFoo {
    static auto foo() -> int { return 1; }
};

struct FooDataMember {
    int foo{};
};

struct PrivateFoo {
  private:
    auto foo() -> int { return 1; }
};

struct DeletedFoo {
    auto foo() -> int = delete;
};

static_assert(rjk::satisfies<HasFoo, Fooable>);
static_assert(!rjk::satisfies<Empty, Fooable>);
static_assert(!rjk::satisfies<HasBar, Fooable>);
static_assert(!rjk::satisfies<FooTakesInt, Fooable>);
static_assert(!rjk::satisfies<FooTakesTwoInts, Fooable>);
static_assert(!rjk::satisfies<FooDataMember, Fooable>);
static_assert(rjk::satisfies<StaticFoo, Fooable>); // static members are callable like members
static_assert(!rjk::satisfies<PrivateFoo, Fooable>);
#if 0
// FINDING-4a (PR #69): a deleted member function makes satisfies<> a hard error
// ("use of deleted function DeletedFoo::foo()" from function_candidate)
// instead of evaluating to false. Can't be pinned: enabling it breaks the build.
static_assert(!rjk::satisfies<DeletedFoo, Fooable>);
#endif
static_assert(!rjk::satisfies<int, Fooable>);
static_assert(!rjk::satisfies<int*, Fooable>);

struct FooableInt {
    auto foo(int) -> int;
};

static_assert(rjk::satisfies<FooTakesInt, FooableInt>);
static_assert(!rjk::satisfies<HasFoo, FooableInt>);          // too few params
static_assert(!rjk::satisfies<FooTakesTwoInts, FooableInt>); // too many params

struct FooableIntInt {
    auto foo(int, int) -> int;
};

static_assert(!rjk::satisfies<FooTakesInt, FooableIntInt>);

// Parameter that cannot be converted
struct FooTakesString {
    auto foo(std::string) -> int { return 1; }
};
static_assert(!rjk::satisfies<FooTakesString, FooableInt>);

// One of several members missing
struct FooBar {
    auto foo() -> int;
    auto bar() -> int;
};
static_assert(!rjk::satisfies<HasFoo, FooBar>);
static_assert(!rjk::satisfies<HasBar, FooBar>);

// Inherited trait members are required too
struct DerivedTrait : Fooable {
    auto bar() -> int;
};
static_assert(!rjk::satisfies<HasFoo, DerivedTrait>);
static_assert(!rjk::satisfies<HasBar, DerivedTrait>);

// Multiple traits: all must be satisfied
static_assert(!rjk::satisfies<HasFoo, Fooable, FooableInt>);

// ---------------------------------------------------------------------------
// Constness
// ---------------------------------------------------------------------------

struct ConstFooable {
    auto foo() const -> int;
};

struct MutableOnlyFoo {
    auto foo() -> int { return 1; }
};

struct ConstFoo {
    auto foo() const -> int { return 1; }
};

static_assert(rjk::satisfies<ConstFoo, ConstFooable>);
static_assert(!rjk::satisfies<MutableOnlyFoo, ConstFooable>);

// const trait: only const members are required, but those still are
struct Holder {
    auto get() const -> int;
    auto set(int) -> void;
};

struct ReadOnly {
    auto get() const -> int { return 1; }
};

struct MutableGetter {
    auto get() -> int { return 1; }
    auto set(int) -> void {}
};

static_assert(rjk::satisfies<ReadOnly, const Holder>);
static_assert(!rjk::satisfies<ReadOnly, Holder>);
static_assert(!rjk::satisfies<MutableGetter, Holder>);
static_assert(!rjk::satisfies<MutableGetter, const Holder>);

// ---------------------------------------------------------------------------
// Reference qualifiers
// ---------------------------------------------------------------------------

struct LvalueFooable {
    auto foo() & -> int;
};

struct RvalueFooable {
    auto foo() && -> int;
};

struct ConstLvalueFooable {
    auto foo() const & -> int;
};

struct LvalueOnlyFoo {
    auto foo() & -> int { return 1; }
};

struct RvalueOnlyFoo {
    auto foo() && -> int { return 1; }
};

struct ConstRvalueOnlyFoo {
    auto foo() const && -> int { return 1; }
};

static_assert(rjk::satisfies<LvalueOnlyFoo, LvalueFooable>);
static_assert(rjk::satisfies<RvalueOnlyFoo, RvalueFooable>);
static_assert(!rjk::satisfies<RvalueOnlyFoo, LvalueFooable>);
static_assert(!rjk::satisfies<LvalueOnlyFoo, RvalueFooable>);
// Unqualified trait members are matched as lvalue-qualified (docs/traits/08_lookup.md)
static_assert(!rjk::satisfies<RvalueOnlyFoo, Fooable>);
static_assert(!rjk::satisfies<ConstRvalueOnlyFoo, ConstLvalueFooable>);
static_assert(!rjk::satisfies<ConstRvalueOnlyFoo, ConstFooable>);

// ---------------------------------------------------------------------------
// noexcept
// ---------------------------------------------------------------------------

struct NoexceptFooable {
    auto foo() noexcept -> int;
};

struct NoexceptFoo {
    auto foo() noexcept -> int { return 1; }
};

static_assert(rjk::satisfies<NoexceptFoo, NoexceptFooable>);
static_assert(rjk::satisfies<NoexceptFoo, Fooable>);
static_assert(!rjk::satisfies<HasFoo, NoexceptFooable>);

struct NoexceptConstFooable {
    auto foo() const noexcept -> int;
};
struct NoexceptConstFoo {
    auto foo() const noexcept -> int { return 1; }
};
static_assert(rjk::satisfies<NoexceptConstFoo, NoexceptConstFooable>);
static_assert(!rjk::satisfies<ConstFoo, NoexceptConstFooable>);

// noexcept member function whose return value conversion may throw
struct NoexceptStringable {
    auto str() noexcept -> std::string;
};

struct ReturnsCharPtr {
    auto str() noexcept -> const char* { return "x"; }
};
static_assert(!rjk::satisfies<ReturnsCharPtr, NoexceptStringable>);

// ---------------------------------------------------------------------------
// Return types
// ---------------------------------------------------------------------------

struct FooReturnsString {
    auto foo() -> std::string { return {}; }
};

struct FooReturnsVoid {
    auto foo() -> void {}
};

static_assert(!rjk::satisfies<FooReturnsString, Fooable>);
static_assert(!rjk::satisfies<FooReturnsVoid, Fooable>);

struct RefFooable {
    auto foo() -> int&;
};

struct FooReturnsValue {
    auto foo() -> int { return 1; }
};

struct FooReturnsConstRef {
    int x{};
    auto foo() -> const int& { return x; }
};

struct FooReturnsRef {
    int x{};
    auto foo() -> int& { return x; }
};

static_assert(rjk::satisfies<FooReturnsRef, RefFooable>);
static_assert(!rjk::satisfies<FooReturnsValue, RefFooable>);
static_assert(!rjk::satisfies<FooReturnsConstRef, RefFooable>);

// ---------------------------------------------------------------------------
// Duck covariance (docs/ducks/04_duck_covariance.md)
// ---------------------------------------------------------------------------

struct Named {
    auto name() const -> int;
};

struct NamedImpl {
    auto name() const -> int { return 1; }
};

struct DuckReturning {
    auto make() -> rjk::duck<Named>;
};

struct ViewReturning {
    auto view() -> rjk::duck_view<Named>;
};

struct ConstViewReturning {
    auto view() -> rjk::duck_view<const Named>;
};

struct PtrReturning {
    auto ptr() -> rjk::duck_ptr<Named>;
};

struct MakeByValue {
    auto make() -> NamedImpl { return {}; }
};

struct MakeByRvalueRef {
    NamedImpl n;
    auto make() -> NamedImpl&& { return std::move(n); }
};

struct MakeByLvalueRef {
    NamedImpl n;
    auto make() -> NamedImpl& { return n; }
};

struct MakeByConstLvalueRef {
    NamedImpl n;
    auto make() -> const NamedImpl& { return n; }
};

struct MakeByConstRvalueRef {
    NamedImpl n;
    auto make() -> const NamedImpl&& { return std::move(n); }
};

struct MakeUnrelated {
    auto make() -> Empty { return {}; }
};

static_assert(rjk::satisfies<MakeByValue, DuckReturning>);
static_assert(rjk::satisfies<MakeByRvalueRef, DuckReturning>);
static_assert(!rjk::satisfies<MakeByLvalueRef, DuckReturning>);
static_assert(!rjk::satisfies<MakeByConstLvalueRef, DuckReturning>);
static_assert(!rjk::satisfies<MakeByConstRvalueRef, DuckReturning>);
static_assert(!rjk::satisfies<MakeUnrelated, DuckReturning>);

struct ViewByLvalueRef {
    NamedImpl n;
    auto view() -> NamedImpl& { return n; }
};

struct ViewByConstLvalueRef {
    NamedImpl n;
    auto view() -> const NamedImpl& { return n; }
};

struct ViewByValue {
    auto view() -> NamedImpl { return {}; }
};

struct ViewByRvalueRef {
    NamedImpl n;
    auto view() -> NamedImpl&& { return std::move(n); }
};

struct ViewUnrelated {
    Empty e;
    auto view() -> Empty& { return e; }
};

static_assert(rjk::satisfies<ViewByLvalueRef, ViewReturning>);
static_assert(rjk::satisfies<ViewByConstLvalueRef, ConstViewReturning>);
static_assert(rjk::satisfies<ViewByLvalueRef, ConstViewReturning>);
static_assert(!rjk::satisfies<ViewByConstLvalueRef, ViewReturning>);
static_assert(!rjk::satisfies<ViewByValue, ViewReturning>);
static_assert(!rjk::satisfies<ViewByRvalueRef, ViewReturning>);
static_assert(!rjk::satisfies<ViewUnrelated, ViewReturning>);

struct PtrByPtr {
    NamedImpl n;
    auto ptr() -> NamedImpl* { return &n; }
};

struct PtrByConstPtr {
    NamedImpl n;
    auto ptr() -> const NamedImpl* { return &n; }
};

struct PtrByRef {
    NamedImpl n;
    auto ptr() -> NamedImpl& { return n; }
};

struct PtrUnrelated {
    Empty e;
    auto ptr() -> Empty* { return &e; }
};

static_assert(rjk::satisfies<PtrByPtr, PtrReturning>);
// FINDING-2 (PR #69): a `const T*` return satisfies a mutable
// `duck_ptr<Traits...>` return (docs say only `duck_ptr<const Traits...>`).
// is_return_compatible checks is_const(remove_reference(ret)) instead of the
// pointee. Actually constructing rjk::duck<PtrReturning>{PtrByConstPtr{}} is then
// a hard error ("Cannot pass const object to duck_view with mutable traits").
// The pin asserts the CURRENT (wrong) behaviour so a fix shows up as a build
// failure: when that happens, delete the pin and enable the #if 0 block.
static_assert(rjk::satisfies<PtrByConstPtr, PtrReturning>); // FINDING-2 pin
#if 0
static_assert(!rjk::satisfies<PtrByConstPtr, PtrReturning>);
#endif
static_assert(!rjk::satisfies<PtrByRef, PtrReturning>);
static_assert(!rjk::satisfies<PtrUnrelated, PtrReturning>);

// ---------------------------------------------------------------------------
// Operators
// ---------------------------------------------------------------------------

struct Negatable {
    auto operator-() -> int;
};

struct LhsAddable {
    auto operator+(int) const -> int;
};

struct RhsSubtractable {
    [[=rjk::right_side]]
    auto operator-(int) const -> int;
};

struct NoOps {};

struct OnlyLhsMinus {
    auto operator-(int) const -> int { return 1; }
};

struct OnlyRhsMinus {
    friend auto operator-(int, const OnlyRhsMinus&) -> int { return 1; }
};

struct MutablePlus {
    auto operator+(int) -> int { return 1; }
};

struct ConstPlus {
    auto operator+(int) const -> int { return 1; }
};

struct PlusReturnsString {
    auto operator+(int) const -> std::string { return {}; }
};

static_assert(rjk::satisfies<int, Negatable>);
static_assert(!rjk::satisfies<NoOps, Negatable>);
static_assert(!rjk::satisfies<std::string, Negatable>);

static_assert(rjk::satisfies<ConstPlus, LhsAddable>);
static_assert(!rjk::satisfies<NoOps, LhsAddable>);
static_assert(!rjk::satisfies<MutablePlus, LhsAddable>);       // constness
static_assert(!rjk::satisfies<PlusReturnsString, LhsAddable>); // return type

static_assert(rjk::satisfies<OnlyRhsMinus, RhsSubtractable>);
static_assert(!rjk::satisfies<OnlyLhsMinus, RhsSubtractable>); // wrong side
static_assert(!rjk::satisfies<OnlyRhsMinus, Negatable>);

struct LhsSubtractable {
    auto operator-(int) const -> int;
};
static_assert(!rjk::satisfies<OnlyRhsMinus, LhsSubtractable>); // wrong side

// Friend with non-const self parameter does not satisfy a const operator
struct MutableFriendRhs {
    friend auto operator-(int, MutableFriendRhs&) -> int { return 1; }
};
static_assert(!rjk::satisfies<MutableFriendRhs, RhsSubtractable>);

// noexcept on regular operators
struct NoexceptLhsAddable {
    auto operator+(int) const noexcept -> int;
};

struct NoexceptConstPlus {
    auto operator+(int) const noexcept -> int { return 1; }
};

static_assert(rjk::satisfies<NoexceptConstPlus, NoexceptLhsAddable>);
static_assert(!rjk::satisfies<ConstPlus, NoexceptLhsAddable>);

struct NoexceptNegatable {
    auto operator-() noexcept -> int;
};

struct ThrowingNegate {
    auto operator-() -> int { return 1; }
};
static_assert(!rjk::satisfies<ThrowingNegate, NoexceptNegatable>);

// Ref-qualified operators
struct RvalueNegatable {
    auto operator-() && -> int;
};

struct LvalueOnlyNegate {
    auto operator-() & -> int { return 1; }
};
static_assert(!rjk::satisfies<LvalueOnlyNegate, RvalueNegatable>);

// operator()
struct CallableInt {
    auto operator()(int) const -> int;
};

struct CallTakesString {
    auto operator()(std::string) const -> int { return 1; }
};

struct MutableCall {
    auto operator()(int) -> int { return 1; }
};

struct CallReturnsString {
    auto operator()(int) const -> std::string { return {}; }
};

struct ConstCall {
    auto operator()(int) const -> int { return 1; }
};

static_assert(rjk::satisfies<ConstCall, CallableInt>);
static_assert(!rjk::satisfies<NoOps, CallableInt>);
static_assert(!rjk::satisfies<CallTakesString, CallableInt>);
static_assert(!rjk::satisfies<MutableCall, CallableInt>);
static_assert(!rjk::satisfies<CallReturnsString, CallableInt>);
static_assert(!rjk::satisfies<int, CallableInt>);

struct CallableIntInt {
    auto operator()(int, int) const -> int;
};
static_assert(!rjk::satisfies<ConstCall, CallableIntInt>); // arity

// operator[]
struct SubscriptableInt {
    auto operator[](int) const -> int;
};

struct ConstSubscript {
    auto operator[](int) const -> int { return 1; }
};

struct MutableSubscript {
    auto operator[](int) -> int { return 1; }
};

struct SubscriptReturnsString {
    auto operator[](int) const -> std::string { return {}; }
};

static_assert(rjk::satisfies<ConstSubscript, SubscriptableInt>);
static_assert(!rjk::satisfies<NoOps, SubscriptableInt>);
static_assert(!rjk::satisfies<MutableSubscript, SubscriptableInt>);
static_assert(!rjk::satisfies<SubscriptReturnsString, SubscriptableInt>);

// noexcept on operator() / operator[]
struct NoexceptCallableInt {
    auto operator()(int) const noexcept -> int;
};

struct NoexceptSubscriptableInt {
    auto operator[](int) const noexcept -> int;
};

struct NoexceptConstCall {
    auto operator()(int) const noexcept -> int { return 1; }
};

static_assert(rjk::satisfies<NoexceptConstCall, NoexceptCallableInt>);

struct NoexceptConstSubscript {
    auto operator[](int) const noexcept -> int { return 1; }
};
static_assert(rjk::satisfies<NoexceptConstSubscript, NoexceptSubscriptableInt>);

// FINDING-1 (PR #69): matches_operator (src/duck_tags.hpp) does not check
// noexcept for operator() / operator[] (known TODO). A throwing operator() /
// operator[] satisfies a trait that declares it noexcept. The pins assert the
// CURRENT (wrong) behaviour so a fix shows up as a build failure: when that
// happens, delete the pins and enable the #if 0 block.
static_assert(rjk::satisfies<ConstCall, NoexceptCallableInt>);           // FINDING-1 pin
static_assert(rjk::satisfies<ConstSubscript, NoexceptSubscriptableInt>); // FINDING-1 pin
#if 0
static_assert(!rjk::satisfies<ConstCall, NoexceptCallableInt>);
static_assert(!rjk::satisfies<ConstSubscript, NoexceptSubscriptableInt>);
#endif

// ---------------------------------------------------------------------------
// Lookup rules (docs/traits/08_lookup.md)
// ---------------------------------------------------------------------------

struct StrictFooable {
    rjk::lookup_rule function_lookup = rjk::lookup_rule::strict;

    auto foo() -> int;
};

struct StrictFooableInt {
    rjk::lookup_rule function_lookup = rjk::lookup_rule::strict;

    auto foo(int) -> int;
};

struct FooTakesDouble {
    auto foo(double) -> int { return 1; }
};

struct FooTakesLong {
    auto foo(long) -> int { return 1; }
};

struct FooTakesConstIntRef {
    auto foo(const int&) -> int { return 1; }
};

struct FooReturnsLong {
    auto foo() -> long { return 1; }
};

// Default lookup: implicit conversions are fine
static_assert(rjk::satisfies<ConstFoo, Fooable>);
static_assert(rjk::satisfies<FooTakesDouble, FooableInt>);
static_assert(rjk::satisfies<FooTakesConstIntRef, FooableInt>);

// Strict lookup: exact signature only
static_assert(rjk::satisfies<HasFoo, StrictFooable>);
static_assert(rjk::satisfies<FooTakesInt, StrictFooableInt>);
static_assert(!rjk::satisfies<ConstFoo, StrictFooable>);
static_assert(rjk::satisfies<NoexceptFoo, StrictFooable>); // noexcept member still matches non-noexcept sig
static_assert(!rjk::satisfies<FooTakesDouble, StrictFooableInt>);
static_assert(!rjk::satisfies<FooTakesLong, StrictFooableInt>);
static_assert(!rjk::satisfies<FooTakesConstIntRef, StrictFooableInt>);
static_assert(!rjk::satisfies<FooReturnsLong, StrictFooable>);
static_assert(!rjk::satisfies<LvalueOnlyFoo, StrictFooable>);
static_assert(!rjk::satisfies<Empty, StrictFooable>);

struct StrictNoexceptFooable {
    rjk::lookup_rule function_lookup = rjk::lookup_rule::strict;

    auto foo() noexcept -> int;
};
static_assert(rjk::satisfies<NoexceptFoo, StrictNoexceptFooable>);
static_assert(!rjk::satisfies<HasFoo, StrictNoexceptFooable>);

// ---------------------------------------------------------------------------
// impl lookup (docs/traits/07_extension.md, docs/traits/08_lookup.md)
// ---------------------------------------------------------------------------

struct ImplConstFooable {
    auto foo() const -> int;
};

struct ImplMutableFooable {
    auto foo() -> int;
};

struct ImplLvalueFooable {
    auto foo() & -> int;
};

// Positive-control targets: same shape as the negative ones below, but with an
// impl that is supposed to match.
struct ImplOkForwarding {};
struct ImplOkConstAuto {};
struct ImplOkConstRef {};
struct ImplOkLvalueRefQualified {};

// Negative targets
struct ImplTargetForwarding {};
struct ImplTargetWrongType {};
struct ImplTargetExtraParam {};
struct ImplTargetExtraParamConst {};
struct ImplTargetMissing {};

// FINDING-7 targets
struct ImplLvalueAuto {};
struct ImplLvalueRef {};

}  // namespace
}  // namespace rjk_test::rejection

namespace rt = rjk_test::rejection;

// --- positive controls ---
template <>
struct rjk::impl<rt::ImplOkForwarding, rt::ImplMutableFooable> {
    static auto foo(auto&&) -> int { return 1; }
};

template <>
struct rjk::impl<rt::ImplOkConstAuto, rt::ImplConstFooable> {
    static auto foo(const auto&) -> int { return 1; }
};

template <>
struct rjk::impl<rt::ImplOkConstRef, rt::ImplConstFooable> {
    static auto foo(const rt::ImplOkConstRef&) -> int { return 1; }
};

template <>
struct rjk::impl<rt::ImplOkLvalueRefQualified, rt::ImplLvalueFooable> {
    static auto foo(rt::ImplOkLvalueRefQualified&) -> int { return 1; }
};

// --- negatives ---

// auto&& implies mutability: does not satisfy a const member
template <>
struct rjk::impl<rt::ImplTargetForwarding, rt::ImplConstFooable> {
    static auto foo(auto&&) -> int { return 1; }
};

#if 0
// FINDING-4b: first parameter is the wrong type -> hard error, see below.
template <>
struct rjk::impl<rt::ImplTargetWrongType, rt::ImplMutableFooable> {
    static auto foo(rt::Empty&) -> int { return 1; }
};
#endif

// Extra parameter. The self parameter is one that is otherwise accepted
// (compare the positive controls), so the extra int is the only reason these
// are rejected.
template <>
struct rjk::impl<rt::ImplTargetExtraParam, rt::ImplMutableFooable> {
    static auto foo(auto&&, int) -> int { return 1; }
};

template <>
struct rjk::impl<rt::ImplTargetExtraParamConst, rt::ImplConstFooable> {
    static auto foo(const rt::ImplTargetExtraParamConst&, int) -> int { return 1; }
};

// Wrong name in impl (otherwise identical to ImplOkForwarding)
template <>
struct rjk::impl<rt::ImplTargetMissing, rt::ImplMutableFooable> {
    static auto bar(auto&&) -> int { return 1; }
};

// --- FINDING-7 ---
template <>
struct rjk::impl<rt::ImplLvalueAuto, rt::ImplMutableFooable> {
    static auto foo(auto&) -> int { return 1; }
};

template <>
struct rjk::impl<rt::ImplLvalueRef, rt::ImplMutableFooable> {
    static auto foo(rt::ImplLvalueRef&) -> int { return 1; }
};

namespace rjk_test::rejection {
namespace {

static_assert(rjk::satisfies<ImplOkForwarding, ImplMutableFooable>);
static_assert(rjk::satisfies<ImplOkConstAuto, ImplConstFooable>);
static_assert(rjk::satisfies<ImplOkConstRef, ImplConstFooable>);
static_assert(rjk::satisfies<ImplOkLvalueRefQualified, ImplLvalueFooable>);
static_assert(rjk::satisfies<ImplOkConstAuto, const ImplConstFooable>);

static_assert(!rjk::satisfies<ImplTargetForwarding, ImplConstFooable>);
static_assert(!rjk::satisfies<ImplTargetExtraParam, ImplMutableFooable>);
static_assert(!rjk::satisfies<ImplTargetExtraParamConst, ImplConstFooable>);
static_assert(!rjk::satisfies<ImplTargetMissing, ImplMutableFooable>);
// The impl is for a different trait, so it doesn't help with this one
static_assert(!rjk::satisfies<ImplOkForwarding, ImplConstFooable>);

// FINDING-4b/4c (PR #69): an impl member whose first parameter is not the
// target type makes satisfies<> a hard error (out-of-bounds dereference in
// qualifiers_of_target, remove_fn_qualifiers.hpp) instead of false. An impl
// member with zero parameters fails the same way ("dereferencing a null
// pointer"). Can't be pinned: enabling it breaks the build.
#if 0
static_assert(!rjk::satisfies<ImplTargetWrongType, ImplMutableFooable>);
#endif

// FINDING-7 (PR #69): an impl whose self parameter is `auto&` or `T&` matches
// only `&`-qualified trait members, never an unqualified mutable one. That
// contradicts docs/traits/07_extension.md, whose example uses
// `static foo(auto& self)` for an unqualified `foo()` and says satisfies<> passes,
// and docs/traits/08_lookup.md, which says unqualified trait members are matched
// as lvalue-qualified. The asserts below pin the CURRENT behaviour so that a fix
// shows up as a build failure: when that happens, delete them and enable the
// #if 0 block.
static_assert(!rjk::satisfies<ImplLvalueAuto, ImplMutableFooable>); // FINDING-7 pin
static_assert(!rjk::satisfies<ImplLvalueRef, ImplMutableFooable>);  // FINDING-7 pin
#if 0
static_assert(rjk::satisfies<ImplLvalueAuto, ImplMutableFooable>);
static_assert(rjk::satisfies<ImplLvalueRef, ImplMutableFooable>);
#endif

// ---------------------------------------------------------------------------
// rjk::copyable
// ---------------------------------------------------------------------------

struct MoveOnlyFoo {
    MoveOnlyFoo() = default;
    MoveOnlyFoo(MoveOnlyFoo&&) = default;
    MoveOnlyFoo(const MoveOnlyFoo&) = delete;
    auto foo() -> int { return 1; }
};

static_assert(rjk::satisfies<MoveOnlyFoo, Fooable>);
static_assert(!rjk::satisfies<MoveOnlyFoo, rjk::copyable>);
static_assert(!rjk::satisfies<MoveOnlyFoo, Fooable, rjk::copyable>);

// ---------------------------------------------------------------------------
// rjk::like
// ---------------------------------------------------------------------------

class IFoo {
  public:
    virtual ~IFoo() = default;
    virtual auto foo() const -> int = 0;
    virtual auto bar(int) -> void = 0;
};

using LikeFoo = rjk::like<IFoo>;
using LikeFooVirtualConst = rjk::like<IFoo, rjk::all_of<std::meta::is_virtual, std::meta::is_const>>;

struct LikeOnlyFoo {
    auto foo() const -> int { return 1; }
};

struct LikeBoth {
    auto foo() const -> int { return 1; }
    auto bar(int) -> void {}
};

static_assert(rjk::satisfies<LikeBoth, LikeFoo>);
static_assert(!rjk::satisfies<LikeOnlyFoo, LikeFoo>);
static_assert(rjk::satisfies<LikeOnlyFoo, LikeFooVirtualConst>);
static_assert(!rjk::satisfies<Empty, LikeFooVirtualConst>);
static_assert(!rjk::satisfies<MutableOnlyFoo, LikeFooVirtualConst>);

// ---------------------------------------------------------------------------
// duck construction / assignment
// ---------------------------------------------------------------------------

using FooDuck = rjk::duck<Fooable>;
using CopyFooDuck = rjk::duck<Fooable, rjk::copyable>;
using ConstFooDuck = rjk::duck<ConstFooable>;

static_assert(std::constructible_from<FooDuck, HasFoo>);
static_assert(!std::constructible_from<FooDuck, Empty>);
static_assert(!std::constructible_from<FooDuck, HasBar>);
static_assert(!std::constructible_from<FooDuck, int>);
static_assert(!std::convertible_to<Empty, FooDuck>);
static_assert(!std::constructible_from<FooDuck, std::in_place_type_t<Empty>>);
static_assert(!std::constructible_from<ConstFooDuck, MutableOnlyFoo>);
static_assert(!std::constructible_from<CopyFooDuck, MoveOnlyFoo>);

static_assert(std::is_assignable_v<FooDuck&, HasFoo>);
static_assert(!std::is_assignable_v<FooDuck&, Empty>);
static_assert(!std::is_assignable_v<FooDuck&, HasBar>);

// in_place construction is explicit
static_assert(!std::convertible_to<std::in_place_type_t<HasFoo>, FooDuck>);

// Copying requires rjk::copyable
static_assert(!std::is_copy_constructible_v<FooDuck>);
static_assert(!std::is_copy_assignable_v<FooDuck>);
static_assert(std::is_move_constructible_v<FooDuck>);
static_assert(std::is_move_assignable_v<FooDuck>);
static_assert(std::is_copy_constructible_v<CopyFooDuck>);
static_assert(std::is_copy_assignable_v<CopyFooDuck>);

// No default constructor (a duck always holds a value)
static_assert(!std::is_default_constructible_v<FooDuck>);

// ---------------------------------------------------------------------------
// duck_view construction
// ---------------------------------------------------------------------------

using FooView = rjk::duck_view<Fooable>;
using ConstFooView = rjk::duck_view<const ConstFooable>;

static_assert(std::constructible_from<FooView, HasFoo&>);
static_assert(!std::constructible_from<FooView, Empty&>);
static_assert(!std::convertible_to<Empty&, FooView>);

// Mutable view cannot bind to a const object
static_assert(!std::constructible_from<FooView, const HasFoo&>);
static_assert(!std::convertible_to<const HasFoo&, FooView>);
static_assert(std::constructible_from<ConstFooView, const ConstFoo&>);

// Arrays and functions can't be viewed
static_assert(!std::constructible_from<rjk::duck_view<>, int (&)[3]>);
static_assert(!std::constructible_from<rjk::duck_view<>, void (&)()>);
static_assert(!std::constructible_from<rjk::duck_view<>, void (*)()>);

// Positive controls for the const-duck finding below
static_assert(std::constructible_from<FooView, FooDuck&>);
static_assert(std::constructible_from<rjk::duck_view<const Fooable>, const FooDuck&>);

// FINDING-3 (PR #69): duck_view's converting constructor from a duck/duck_view
// is constrained only on can_convert_from, which ignores constness of the
// source. The constraint is satisfied, so these traits report true, but the
// constructor body is a hard error ("invalid conversion from const void* to
// void*"). Expected: SFINAE-friendly rejection. The pins assert the CURRENT
// (wrong) behaviour so a fix shows up as a build failure: when that happens,
// delete the pins and enable the #if 0 blocks.
static_assert(std::constructible_from<FooView, const FooDuck&>);                // FINDING-3 pin
static_assert(std::constructible_from<FooView, rjk::duck_view<const Fooable>>); // FINDING-3 pin
#if 0
static_assert(!std::constructible_from<FooView, const FooDuck&>);
static_assert(!std::convertible_to<const FooDuck&, FooView>);
// Mutable view from a const view
static_assert(!std::constructible_from<FooView, rjk::duck_view<const Fooable>>);
#endif

// ---------------------------------------------------------------------------
// Trait narrowing (docs/ducks/03_trait_narrowing.md)
// ---------------------------------------------------------------------------

struct TraitA {
    auto a() -> int;
};

struct TraitB {
    auto b() -> int;
};

struct TraitC {
    auto c() -> int;
};

struct Unrelated {
    auto u() -> int;
};

using ABDuck = rjk::duck<TraitA, TraitB>;
using ABCDuck = rjk::duck<TraitA, TraitB, TraitC>;

// Positive controls
static_assert(std::convertible_to<ABDuck&, rjk::duck_view<TraitA>>);
static_assert(std::convertible_to<ABDuck&, rjk::duck_view<const TraitA>>);
static_assert(std::convertible_to<ABDuck&, rjk::duck_view<const TraitA, const TraitB>>);
static_assert(std::convertible_to<ABDuck&&, rjk::duck<TraitB, TraitA>>); // permutation

// Narrowing to an unrelated trait
static_assert(!std::constructible_from<rjk::duck_view<Unrelated>, ABDuck&>);
static_assert(!std::constructible_from<rjk::duck<Unrelated>, ABDuck&&>);
static_assert(!std::constructible_from<rjk::duck_view<TraitA, Unrelated>, ABDuck&>);
static_assert(!std::constructible_from<rjk::duck_view<TraitC>, ABDuck&>);

// Widening is not narrowing
static_assert(!std::constructible_from<rjk::duck_view<TraitA, TraitB>, rjk::duck<TraitA>&>);
static_assert(!std::constructible_from<rjk::duck<TraitA, TraitB>, rjk::duck<TraitA>&&>);

// A duck can't be constructed from a duck_view, even with rjk::copyable: it
// would silently copy the viewed object.
template <typename V>
concept deduces_duck_from = requires(V v) { rjk::duck{v}; };
static_assert(!std::constructible_from<rjk::duck<TraitA, rjk::copyable>, rjk::duck_view<TraitA, rjk::copyable>>);
static_assert(!std::constructible_from<rjk::duck<TraitA, rjk::copyable>, rjk::duck_view<TraitA, TraitB, rjk::copyable>>);
static_assert(!deduces_duck_from<rjk::duck_view<TraitA, rjk::copyable>>);

// const -> mutable is not narrowing
// FINDING-3 (PR #69), continued: single-trait narrowing compares
// remove_const(trait), so a const source passes the constraint and fails in the
// constructor body.
static_assert(std::constructible_from<rjk::duck_view<TraitA>, rjk::duck_view<const TraitA, const TraitB>>); // FINDING-3 pin
static_assert(std::constructible_from<rjk::duck_view<TraitA>, const ABDuck&>);                              // FINDING-3 pin
#if 0
static_assert(!std::constructible_from<rjk::duck_view<TraitA>, rjk::duck_view<const TraitA, const TraitB>>);
static_assert(!std::constructible_from<rjk::duck_view<TraitA>, rjk::duck<const TraitA, const TraitB>&>);
static_assert(!std::constructible_from<rjk::duck_view<TraitA>, const ABDuck&>);
#endif

// duck -> duck narrowing is explicit only (via make_narrowed). The narrowing
// constructors are private, so these also hold trivially; they guard against
// someone making one of them public and implicit.
static_assert(!std::convertible_to<ABDuck&&, rjk::duck<TraitA>>);
static_assert(!std::convertible_to<ABDuck&&, rjk::duck<const TraitA, const TraitB>>);

// Multi-trait narrowing is not supported (docs/ducks/03_trait_narrowing.md)
static_assert(!std::constructible_from<rjk::duck_view<TraitA, TraitB>, ABCDuck&>);


// make_narrowed is callable for valid narrowing conversions
template <typename Duck, typename... NewTraits>
concept can_make_narrowed = requires(Duck&& d) {
    rjk::make_narrowed<NewTraits...>(std::forward<Duck>(d));
};
struct CopyableTraitA : TraitA, rjk::copyable {};
struct CopyableTraitB : TraitB, rjk::copyable {};
using CopyABDuck = rjk::duck<CopyableTraitA, CopyableTraitB>;

static_assert(can_make_narrowed<ABDuck, TraitA>);   // move-narrowing
static_assert(can_make_narrowed<ABDuck&&, TraitA>);
// Copy-narrowing from a const lvalue works when the traits are copyable
static_assert(can_make_narrowed<const CopyABDuck&, CopyableTraitA>);
// make_narrowed to an invalid target is a hard error by design, not a
// substitution failure: the target traits are always spelled out explicitly,
// so a mismatch is a user error (like std::make_unique<T>(args...) with
// unconstructible args). These are therefore deliberately not checked here:
//   make_narrowed<Unrelated>(ABDuck{})            // trait not in the source
//   make_narrowed<TraitA>(std::as_const(abDuck))  // copy-narrowing, not copyable
//   make_narrowed<TraitA>(std::as_const(copyAB))  // target trait not copyable
// A clearer diagnostic for these cases would fit under #64.

// ---------------------------------------------------------------------------
// get / get_if / emplace with non-satisfying types
// ---------------------------------------------------------------------------

template <typename T, typename Duck>
concept can_get = requires(Duck& d) { rjk::get<T>(d); };

template <typename T, typename Duck>
concept can_get_if = requires(Duck* d) { rjk::get_if<T>(d); };

template <typename T, typename Duck>
concept can_emplace = requires(Duck& d) { rjk::emplace<T>(d); };

static_assert(can_get<HasFoo, FooDuck>);
static_assert(!can_get<int, FooDuck>);
static_assert(!can_get<Empty, FooDuck>);
static_assert(!can_get_if<Empty, FooDuck>);
static_assert(can_get<HasFoo, FooView>);
static_assert(!can_get<Empty, FooView>);
static_assert(can_emplace<HasFoo, FooDuck>);
static_assert(!can_emplace<Empty, FooDuck>);

// ---------------------------------------------------------------------------
// Calling members through a duck
// ---------------------------------------------------------------------------

template <typename D>
concept can_call_foo = requires(D d) { d.foo(); };

template <typename D>
concept can_call_get = requires(D d) { d.get(); };

template <typename D>
concept can_call_set = requires(D d) { d.set(1); };

// Mutable member not callable on const duck
static_assert(can_call_foo<FooDuck&>);
static_assert(!can_call_foo<const FooDuck&>);
static_assert(!can_call_foo<rjk::duck_view<const Fooable>>);

// const trait hides mutable members
static_assert(can_call_get<rjk::duck<const Holder>&>);
static_assert(!can_call_set<rjk::duck<const Holder>&>);
static_assert(!can_call_set<rjk::duck_view<const Holder>>);

// ref-qualified
template <typename D>
concept can_call_foo_rvalue = requires(D&& d) { std::forward<D>(d).foo(); };

static_assert(can_call_foo_rvalue<rjk::duck<RvalueFooable>>);
static_assert(!can_call_foo_rvalue<rjk::duck<RvalueFooable>&>);
static_assert(!can_call_foo_rvalue<rjk::duck<LvalueFooable>>);

// wrong arity / argument type through the duck
template <typename D>
concept can_call_foo_int = requires(D d) { d.foo(1); };
template <typename D>
concept can_call_foo_str = requires(D d) { d.foo(std::string{}); };

static_assert(!can_call_foo_int<FooDuck&>);
static_assert(can_call_foo_int<rjk::duck<FooableInt>&>);
static_assert(!can_call_foo_str<rjk::duck<FooableInt>&>);

// operator not provided by trait is not available
template <typename D>
concept can_add = requires(D d) { d + 1; };
static_assert(!can_add<FooDuck&>);
static_assert(can_add<rjk::duck<LhsAddable>&>);
static_assert(!can_add<rjk::duck<RhsSubtractable>&>);

TEST(Rejection, CompileTimeChecks) {
    SUCCEED();
}

}  // namespace
}  // namespace rjk_test::rejection
