# Operators

Traits can also enforce that a type defines certain operators. Here's a simple example with a unary operator:

```c++
struct Negatable {
    auto operator-() -> int;
};

static_assert(rjk::satisfies<int, Negatable>);
```

It respects any type for which the operator is valid, not just those that define it as a member function.

For binary operators, there's a few ways to define them:

```c++
// Matches any type that can be used like 'x + 5'
struct LhsAddable {
    auto operator+(int) const -> int;
};

// Matches any type that can be used like '5 - x'
struct RhsSubtractable {
    [[=rjk::right_side]]
    auto operator-(int) const -> int;
};
```

the `right_side` annotation is useful for matching against types that overload operators using friends. For example:

```c++
struct Printable {
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

> [!CAUTION]
> Defining an interface that supports both the left- and right-hand side versions
> of an operator can be tricky if they have the same signature. To workaround this,
> consider placing the functions in two different traits and composing them (see [composition.md](06_composition.md))
> for more details.