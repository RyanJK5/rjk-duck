# Policies

**Policies** are a type of trait defined using different syntax. Specifically, policies are
specified by using `rjk::policy` along with template arguments called **tags**.

In general, they can be used identically to traits defined using the `trait` annotation.

> [!IMPORTANT]
> In general, `struct`-style traits should be preferred to policies, with
> the exception of `copyable` (see below). Policies are available to make
> metaprogramming with `rjk::duck` more achievable, since it is currently
> impossible to generate `struct`-style traits using reflection.

## Basic Syntax

```c++
using MyTrait = rjk::policy<
    rjk::has_fn<"foo", auto() -> int>,
    rjk::has_fn<"bar", auto(int) const -> int>
>;
```

The policy above is equivalent to:

```c++
struct [[=rjk::trait]] MyTrait {
    auto foo() -> int;
    auto bar(int input) const -> int;
};
```

This demonstrates the `has_fn` tag, which simply requires that a member function is present
with the provided identifier and signature.

## Operators

Policies use the `has_op` tag to define operators. Rather than using annotations like for regular traits, policies define operator function signatures
using a dummy parameter called `self`.

```c++
using Addable = rjk::policy<
    rjk::has_op<rjk::op_plus, auto(const rjk::self&, int) -> int>,
    rjk::has_op<rjk::op_plus, auto(int, const rjk::self&) -> int>
>;

static_assert(rjk::satisfies<int, Addable>);
static_assert(rjk::satisfies<int, const Addable>);
```

The above is equivalent to:

```c++
struct [[=rjk::trait]] Addable {
    [[=rjk::both_sides]]
    auto operator+(int) const -> int;
};
```

Operators where the operand is the left hand side can also be defined using
member function syntax by omitting `self`:

```c++
using Negatable = rjk::policy<
    rjk::has_op<rjk::op_minus, int() const>
>;
```

Equivalent to:

```c++
struct [[=rjk::trait]] Negatable {
    auto operator-() const -> int;
};
```

## `copyable` and `copy_tag`

By default, traits make no assumption about the movability or copyability of the types they
are checked against. By using the `copyable` policy as a trait, you can require that a type
be copyable.

```c++
static_assert(rjk::is_trait<rjk::copyable>);

static_assert(rjk::satisfies<int, rjk::copyable>);
static_assert(!rjk::satisfies<std::unique_ptr<int>, rjk::copyable>);
```

`copyable` internally is just an alias:

```c++
using copyable = policy<copy_tag>;
```

`copy_tag` can therefore be composed as a tag in policies. In general, though,
the `rjk::copyable` policy covers most use cases.
