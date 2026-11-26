# `like` and Predicates

Make a trait from an existing type. See [05_like.md](../traits/05_like.md).

## `like`

```c++
template <typename T,
          is_meta_predicate auto Predicate = [](std::meta::info) consteval { return true; }>
struct like {};
```

A trait made from `T`'s public, user-declared member functions and member
operators for which `Predicate` returns `true`.

## `is_meta_predicate`

```c++
template <typename Func>
concept is_meta_predicate = std::invocable<Func, std::meta::info> &&
                            std::same_as<std::invoke_result_t<Func, std::meta::info>, bool>;
```

A function taking a member's `std::meta::info` and returning `bool`.

## `all_of`, `any_of`, `none_of`

```c++
template <is_meta_predicate auto... Predicates>
constexpr auto all_of = /* ... */;                                      // (1)

template <is_meta_predicate auto... Predicates>
constexpr auto any_of = /* ... */;                                      // (2)

template <is_meta_predicate auto... Predicates>
constexpr auto none_of = /* ... */;                                     // (3)
```

1. A predicate that is `true` when all of `Predicates` are.
2. A predicate that is `true` when any of `Predicates` is.
3. A predicate that is `true` when none of `Predicates` is.

## `include`, `exclude`

```c++
template <fixed-string... Names>
constexpr auto include = /* ... */;                                     // (1)

template <fixed-string... Names>
constexpr auto exclude = /* ... */;                                     // (2)
```

`Names` are string literals, e.g. `rjk::include<"foo", "bar">`.

1. A predicate that is `true` for members named one of `Names`.
2. A predicate that is `true` for members not named any of `Names`.
