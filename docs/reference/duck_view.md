# `duck_view`

```c++
template <typename... Traits>
class duck_view;
```

Refers to an object, or to a `duck`'s object, that satisfies `Traits...`,
without owning it. See [01_overview.md](../ducks/01_overview.md).

## Constructors

```c++
template <typename T>
duck_view(T&& obj) noexcept;                                            // (1)

template <typename Duck>
duck_view(Duck&& d) noexcept;                                           // (2)

duck_view(const duck_view& other) noexcept;                             // (3)
```

1. Refers to `obj`. Deleted if `T` doesn't satisfy `Traits...`, if `obj` is
   `const` and not every trait is, or if `obj` is a function or an array.
2. Refers to the object of the `duck` or `duck_view` `d`. `Traits...` may be
   `d`'s traits, reordered, all made `const`, or a single one of them
   (optionally `const`). See
   [03_trait_narrowing.md](../ducks/03_trait_narrowing.md). Not available if
   `d` is `const`, or has only `const` traits, and not every trait in
   `Traits...` is `const`.
3. Refers to `other`'s object.

`duck_view` has no default constructor.

## Assignment

```c++
duck_view& operator=(const duck_view& other) noexcept;
```

Refers to `other`'s object instead. Combined with (1) and (2), this rebinds a
view to an object or a `duck`.

## Member functions

```c++
/* each member function and operator of Traits..., with the same signature */
```

Calls the object's matching function. Calling one on a view of a valueless
`duck` is undefined behaviour, and asserts in debug builds.

## Non-member functions

### `get`, `get_if`, `typeid_of`

See [access.md](access.md).

## Deduction guides

```c++
template <typename... Traits>
duck_view(duck<Traits...>&) -> duck_view<Traits...>;                    // (1)

template <typename... Traits>
duck_view(duck<Traits...>&&) -> duck_view<Traits...>;                   // (1)

template <typename... Traits>
duck_view(const duck<Traits...>&) -> duck_view<const Traits...>;        // (2)

template <typename T>
duck_view(T&&) -> duck_view<>;                                          // (3)
```

1. A view of a `duck`'s traits.
2. A view of a `const` `duck`'s traits, all made `const`.
3. A view of any other object, with no traits.
