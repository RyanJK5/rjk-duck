# Operators

Traits can also enforce that a type defines certain operators. Here's a simple example with a unary operator:

```c++
struct [[=rjk::trait]] Negatable {
    auto operator-() -> int;
};

static_assert(rjk::satisfies<int, Negatable>);
```

It respects any type for which the operator is valid, not just those that define it as a member function.

For binary operators, there's a few ways to define them:

```c++
// Matches any type that can be used like 'x + 5'
struct [[=rjk::trait]] LhsAddable {
    auto operator+(int) const -> int;
};

// Matches any type that can be used like '5 - x'
struct [[=rjk::trait]] RhsSubtractable {
    [[=rjk::right_side]]
    auto operator-(int) const -> int;
};

// Matches any type that can be used like 'x / 5' AND '5 / x'.
// This does NOT require that the operator is commutative, just
// that both sides are defined.
struct [[=rjk::trait]] BothDividable {
    [[=rjk::both_sides]]
    auto operator/(int) const -> int;
};
```

the `right_side` annotation and `both_sides` annotations are useful for
matching against types that overload operators using friends. For example:

```c++
struct [[=rjk::trait]] Printable {
    [[=rjk::right_side]]
    auto operator<<(std::ostream& out) const -> std::ostream&;
};

struct Foo {
    friend auto operator<<(std::ostream& out, const Foo& foo) -> std::ostream& {
        out << "foo";
        return out;
    }
};

static_assert(rjk::satisfies<int, Printable>);
static_assert(rjk::satisfies<Foo, Printable>);
```

Note that `const`-ness is still checked here. If `Foo` declared `operator<<` with a
non-`const` `Foo&` as a friend, it would fail to satisfy `Printable`.