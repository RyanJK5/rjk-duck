# Traits

A trait is a class whose member functions and operators describe an interface.
See [01_overview.md](../traits/01_overview.md).

## `satisfies`

```c++
template <typename T, typename... Traits>
concept satisfies = /* ... */;
```

Whether `T` can be stored in a `duck<Traits...>`: every member of every trait
can be called on a `T`, through `T`'s members, free operators or
[`impl`](#impl).

## `copyable`

```c++
struct copyable {
    copyable(const copyable&) = default;
};
```

A trait requiring a copy constructor, which makes a `duck` copyable. It can be
passed to a `duck` or used as a base of a trait.

## `rtti`

```c++
struct rtti {};
```

A trait that stores the object's `std::type_info`, enabling
[`get`, `get_if` and `typeid_of`](access.md). It can be passed to a `duck` or
used as a base of a trait. `const rtti` is the same as `rtti`. It is a compile
error with `-fno-rtti`.

## `impl`

```c++
template <typename T, typename Trait>
struct impl {};
```

Specialize to make `T` satisfy `Trait`'s member functions, through static
functions taking the object first. Operators aren't supported. See
[07_extension.md](../traits/07_extension.md).

## `lookup_rule`

```c++
enum struct lookup_rule {
    none = 0,                                                           // (1)
    strict = 1,                                                         // (2)
};
```

How a trait's member functions are matched, set with a
`rjk::lookup_rule function_lookup` data member in the trait. See
[08_lookup.md](../traits/08_lookup.md#custom-lookup-rules).

1. Overload resolution, as for a regular call. The default.
2. An exact signature match, still allowing duck-covariant returns.
