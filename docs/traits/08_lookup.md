# Lookup

By default, traits will match according to loose lookup rules, similar
to how overload resolution regularly works in C++. Below are some common
examples that work implicitly.

```c++
struct Trait {
    auto foo() -> int;
};

struct A {
    auto foo() const -> int;
};

// Implicit cvref-conversion
static_assert(rjk::satisfies<A, Trait>);
```

```c++
struct AddableWithDouble {
    auto operator+(double) -> double;
};

// Implicit conversions for integral types
static_assert(rjk::satisfies<int, double>);
```

```c++
struct Trait {
    auto foo(int) -> int;
};

struct A {
    auto foo(double) -> int { return 1; }
    auto foo(int) -> int { return 2; }
};

struct B {
    auto foo(double) -> int { return 3; }
};

A a{};
rjk::duck_view<Trait>{a}.foo(5); // 2, the best match is foo(int)

B b{};
rjk::duck_view<Trait>{b}.foo(5); // 3, the best match is foo(double)
```

## Member Function Templates

Member function templates, including members with an explicit object
parameter like `auto foo(this auto&& self)`, can satisfy a trait member:

```c++
struct Trait {
    auto foo(int) -> int;
};

struct A {
    template <typename T>
    auto foo(T t) -> int { return static_cast<int>(t) + 1; }
};

A a{};
rjk::duck_view<Trait>{a}.foo(5); // 6, calls foo<int>
```

Templates and non-templates can be ranked together, like usual:
```c++
struct B {
    auto foo(long) -> int { return 1; }

    template <typename T>
    auto foo(T) -> int { return 2; }
};

B b{};
rjk::duck_view<Trait>{b}.foo(5); // 2, as b.foo(5)
```

See [Limitations](#limitations) for the cases where the ranking differs from C++.

## Reference-Qualified Members

Consider the following:

```c++
struct Trait {
    auto foo() -> int;
};

struct MyType {
    auto foo() & -> int { return 5; }
    
    auto foo() && -> int { return 10; }
};

rjk::duck<Trait> d{MyType{}};
d.foo(); // 5
std::move(d).foo(); // Also 5
```

An unqualified member function of a trait is implicitly lvalue-reference
qualified during the matching process. This is to prevent every unqualified
member function from requiring an lvalue and rvalue overload in the vtable.
If you wish for both overloads to be reachable, consider expressing that like
so:

```c++
struct Trait {
    auto foo() & -> int;
    auto foo() && -> int;
};

rjk::duck<Trait> d{MyType{}};
d.foo(); // 5
std::move(d).foo(); // 10
```

## Custom Lookup Rules

Traits can specify a custom lookup rule if they want stricter matching.
Currently, this is only possible for member functions, and not for operators.
It can be used as follows:

```c++
struct Trait {
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
duck-covariant returns). `strict` never matches
[member function templates](#member-function-templates), since they don't have
an exact signature.

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
struct Trait {
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

### Partial Ordering of Member Templates

If more than one member template can be called, the type doesn't satisfy the
trait, even when the compiler would pick one of them by partial ordering:
```c++
struct Trait {
    auto foo(int*) -> int;
};

struct A {
    template <typename T>
    auto foo(T) -> int { return 1; }

    template <typename T>
    auto foo(T*) -> int { return 2; }
};

int i{};
A{}.foo(&i); // 2, foo(T*) is more specialized
static_assert(!rjk::satisfies<A, Trait>);
```

### Ranking Member Templates

A templated member is ranked as if
it took every argument by reference and matched each one exactly. That differs
from regular overload resolution in two cases:
- **A parameter that doesn't depend on a template parameter** counts as an
  exact match even when the argument needs a conversion. With
  `foo(long, int)` and `template <typename T> foo(T, long)`, calling `foo(1, 2)` is ambiguous according to the compiler, but the templated candidate wins in `duck`.
- **Reference bindings.** With `foo(const int&)` and
  `template <typename T> foo(T)` called with an `int` rvalue, regular overload resolution picks the non-template, but `duck`'s overload resolution picks the template.
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

struct Fooable {
    auto foo() -> int;
};

static_assert(rjk::satisfies<C, Fooable>); // ERROR
```

Currently, it is not possible to reflect over `using` base declarations.
Therefore, the example above will fail with an ambiguous call.
