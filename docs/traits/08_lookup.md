# Lookup

By default, traits will match according to loose lookup rules, similar
to how overload resolution regularly works in C++. Below are some common
examples that work implicitly.

```c++
struct [[=rjk::trait]] Trait {
    auto foo() -> int;
};

struct A {
    auto foo() const -> int;
};

// Implicit cvref-conversion
static_assert(rjk::satisfies<A, Trait>);
```

```c++
struct [[=rjk::trait]] AddableWithDouble {
    [[=rjk::both_sides]]
    auto operator+(double) -> double;
};

// Implicit conversions for integral types
static_assert(rjk::satisfies<int, double>);
```

```c++
struct [[=rjk::trait]] Trait {
    auto foo(int) -> int;
};

struct A {
    constexpr auto foo(double) -> int { return 1; }
    constexpr auto foo(int) -> int { return 2; }
};

struct B {
    constexpr auto foo(double) -> int { return 3; }
};

// Finds best matching function ( foo(int) )
static_assert(rjk::duck_view<Trait>{A{}}.foo(5) == 2);

// Best match is foo(double)
static_assert(rjk::duck_view<Trait>{B{}}.foo(5) == 3);
```

## Custom Lookup Rules

Traits can specify a custom lookup rule if they want stricter matching.
Currently, this is only possible for member functions, and not for operators.
It can be used as follows:

```c++
struct [[=rjk::trait]] Trait {
    rjk::lookup_rule function_lookup = rjk::lookup_rule::strict;
    
    auto foo() -> int;
};

struct A {
    auto foo() const -> int;
};

static_assert(!rjk::satisfies<A, Trait>);
```

Currently, `lookup_rule::none` and `lookup_rule::strict` are provided, with
more customization planned. `none` will use default overload resolution,
and `strict` will search for an *exact* signature match (but still allows
duck-covariant returns).

## `impl` Lookup

Currently, lookup in a specialization of `impl` (see [extension.md](07_extension.md))
will always use strict matching. It will also expect the type being specialized to be
the first template argument of each extended function if it is a function template. For
example:

```c++
// ,,,

template <>
struct rjk::impl<A, Foo> {
    // tries substituting A into foo
    static auto foo(const auto& self) -> int {
        return 5;
    }
};
```

This decision was deliberately intended to allow the use of `auto` for the self
parameter. If the function is templated in any other way, it will not match during
lookup.

Non-template functions can also be matched, so long as they have the correct
type in the first position:

```c++
template <>
struct rjk::impl<A, Foo> {
    static auto foo(const A& self) -> int {
        return 5;
    }
};
```

### Forwarding References in `impl`

Typically, a forwarding reference can match both `const` and non-`const`
types:

```c++
auto foo(auto&&) -> bool;

int x{};
const int y{};
foo(x);
foo(y);
```

When specializing `impl`, the mental model is slightly different. Use forwarding
references to specialize any *mutable* member function, and use `const auto&` for
any `const` function.

One might, for example, expect the following to work:

```c++
struct [[=rjk::trait]] Trait {
    auto foo() const -> int;
};

struct A{};

template <>
struct rjk::impl<A, Trait> {
    static auto foo(auto&& self) -> int;
};

static_assert(rjk::satisfies<A, Trait>);
```

However, because `auto&&` implies mutability in an `impl` specialization,
the correct approach would be to write the specialization as follows:

```c++
template <>
struct rjk::impl<A, Trait> {
    static auto foo(const auto& self) -> int;
};
```

## Limitations

Due to the current state of C++26 reflection, some aspects of overload resolution
remain unresolved.

### Template Argument Deduction

```c++
struct [[=rjk::trait]] Fooable {
    auto foo(int) -> void;
};

struct A {
    template <typename T>
    auto foo(T obj) -> void {}
};

static_assert(rjk::satisfies<A, Fooable>); // ERROR
```

Reflection currently does not provide a feasible way to generically
handle template argument deduction when resolving overloads. Proposals for
features such as `declcall` may make this possible in the future.

### `using` Base Declarations

```c++
struct A {
    constexpr auto foo() -> int { return 1; }
};

struct B {
    constexpr auto foo() -> int { return 2; }
};

struct C : A, B {
    using A::foo;
};

static_assert(C{}.foo() == 1);

struct [[=rjk::trait]] Fooable {
    auto foo() -> int;
};

static_assert(rjk::satisfies<C, Fooable>); // ERROR
```

Currently, it is not possible to reflect over `using` base declarations.
Therefore, the example above will fail with an ambiguous call.