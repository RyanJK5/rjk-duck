# `duck_view`

```c++
template <typename... Traits>
class duck_view;
```

A non-owning reference to an object, or to a `duck`'s object, that satisfies
`Traits...`. Copying a view copies the reference. A view never outlives what
it refers to. See [01_overview.md](../ducks/01_overview.md).

## Constructors

```c++
template <typename T>
duck_view(T&& obj) noexcept;                                       // (1)

template <typename Duck>
duck_view(Duck&& d) noexcept;                                      // (2)
```

1. Refers to `obj`. Deleted if `T` doesn't satisfy `Traits...`, if `obj` is
   `const` and a trait isn't, or if `obj` is a function or array.
2. Refers to the object of a `duck` or `duck_view`. Its traits may be the same
   as `d`'s, reordered, all made `const`, or one of them (optionally `const`).
   See [03_trait_narrowing.md](../ducks/03_trait_narrowing.md). A mutable view
   can't be made from a `const` source.

Assigning another view, object or `duck` rebinds the view.

## Member functions

Every member function and operator of `Traits...`. Calling one on a view of a
valueless `duck` is undefined behaviour, and asserts in debug builds.

## Deduction guides

```c++
template <typename... Traits>
duck_view(duck<Traits...>&) -> duck_view<Traits...>;
template <typename... Traits>
duck_view(duck<Traits...>&&) -> duck_view<Traits...>;
template <typename... Traits>
duck_view(const duck<Traits...>&) -> duck_view<const Traits...>;
template <typename T>
duck_view(T&&) -> duck_view<>;
```

## Non-member functions

`get`, `get_if` and `typeid_of`: see [access.md](access.md).
