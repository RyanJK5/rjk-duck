# `duck`

```c++
template <typename... Traits>
class duck;
```

Owns an object of any type that satisfies `Traits...`. Small objects are stored
in a buffer inside the `duck`, others are allocated. A trait may not appear
twice. See [01_overview.md](../ducks/01_overview.md).

## Member types

| Type | Definition |
|---|---|
| `allocator_type` | The [`allocator` option](options.md). `std::allocator<std::byte>` by default. |

## Constructors

```c++
template <typename T>
duck(T&& obj);                                                     // (1)

template <typename T, typename... Args>
explicit duck(std::in_place_type_t<T>, Args&&... args);            // (2)

template <typename T, typename U, typename... Args>
explicit duck(std::in_place_type_t<T>, std::initializer_list<U> il,
              Args&&... args);                                     // (3)

template <typename Duck>
duck(Duck&& d);                                                    // (4)

duck(const duck& other);                                           // (5)
duck(duck&& other);                                                // (6)
```

1. Stores a decayed copy of `obj`. Deleted if `T` doesn't satisfy `Traits...`.
2. Constructs a `T` from `args...` in place.
3. Constructs a `T` from `il, args...` in place.
4. Takes over the object of a `duck` whose traits are a permutation of `Traits...`.
5. Copies the object. Only available with [`copyable`](traits.md#copyable).
6. Moves the object, leaving `other` valueless.

(1)–(4) also have an `(std::allocator_arg_t, const allocator_type& alloc, ...)`
overload, as do (5) and (6), which use `alloc` instead. Without it,
`allocator_type` must be default-constructible.

## Assignment

```c++
template <typename T>
duck& operator=(T&& obj);
duck& operator=(const duck& other);
duck& operator=(duck&& other);
```

Replaces the object, like the matching constructor. Copy assignment requires
[`copyable`](traits.md#copyable). Allocators propagate as
`std::allocator_traits` says.

## Member functions

Every member function and operator of `Traits...`, with the same signature.
Calling one on a valueless `duck` is undefined behaviour, and asserts in debug
builds.

## Deduction guides

```c++
template <typename T>
duck(T&&) -> duck<>;
```

## Non-member functions

### `make_narrowed`

```c++
template <typename... NewTraits, typename Duck>
duck<NewTraits...> make_narrowed(Duck&& src);

template <typename... NewTraits, typename Duck>
duck<NewTraits...> make_narrowed(std::allocator_arg_t,
                                 const typename duck<NewTraits...>::allocator_type& alloc,
                                 Duck&& src);
```

Copies or moves `src`'s object into a `duck` of a subset of its traits. See
[03_trait_narrowing.md](../ducks/03_trait_narrowing.md).

### `swap`

```c++
template <typename... Traits>
void swap(duck<Traits...>& lhs, duck<Traits...>& rhs);
```

Swaps the objects of two ducks.

### `is_valueless`

```c++
template <typename... Traits>
bool is_valueless(const duck<Traits...>& d) noexcept;
```

Whether `d` holds no object, e.g. after being moved from.

### `emplace`

```c++
template <typename T, typename Duck, typename... Args>
T& emplace(Duck&& d, Args&&... args);

template <typename T, typename U, typename Duck, typename... Args>
T& emplace(Duck&& d, std::initializer_list<U> il, Args&&... args);
```

Replaces `d`'s object with a `T` constructed from the arguments, and returns it.
`d` must be a non-`const` `duck`.

### `get_allocator`

```c++
template <typename Duck>
typename std::decay_t<Duck>::allocator_type get_allocator(const Duck& d) noexcept;
```

Returns a copy of `d`'s allocator.

### `get`, `get_if`, `typeid_of`

See [access.md](access.md).
