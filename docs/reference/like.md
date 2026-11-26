# `like` and Predicates

See [05_like.md](../traits/05_like.md).

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
constexpr auto all_of;
template <is_meta_predicate auto... Predicates>
constexpr auto any_of;
template <is_meta_predicate auto... Predicates>
constexpr auto none_of;
```

Predicates that are `true` when all, any, or none of `Predicates` are.

## `include`, `exclude`

```c++
template <fixed-string... Names>
constexpr auto include;
template <fixed-string... Names>
constexpr auto exclude;
```

Predicates that are `true` for members named, or not named, one of `Names`,
given as string literals: `rjk::include<"foo", "bar">`.
