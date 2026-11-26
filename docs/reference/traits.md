# Traits

A trait is a class whose member functions and operators describe an interface.
See [01_overview.md](../traits/01_overview.md).

## `satisfies`

```c++
template <typename T, typename... Traits>
concept satisfies;
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

A trait requiring a copy constructor. Makes a `duck` copyable. Can be passed to
a `duck` or used as a base of a trait.

## `rtti`

```c++
struct rtti {};
```

A trait that stores the object's `std::type_info`, enabling [`get`](access.md#get),
[`get_if`](access.md#get_if) and [`typeid_of`](access.md#typeid_of). Can be
passed to a `duck` or used as a base of a trait. `const rtti` is the same as
`rtti`. A compile error with `-fno-rtti`.

## `impl`

```c++
template <typename T, typename Trait>
struct impl {};
```

Specialize to make `T` satisfy `Trait`'s member functions, through static
functions taking `self` first. Doesn't support operators. See
[07_extension.md](../traits/07_extension.md).

## `lookup_rule`

```c++
enum struct lookup_rule { none = 0, strict = 1 };
```

How a trait's member functions are matched, set with a
`rjk::lookup_rule function_lookup` data member in the trait. `none` uses
overload resolution, `strict` needs an exact signature. See
[08_lookup.md](../traits/08_lookup.md#custom-lookup-rules).
