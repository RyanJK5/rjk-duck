# `duck`

```c++
template <typename... Traits>
class duck;
```

Owns an object of any type that satisfies `Traits...`. Small objects are stored
in a buffer inside the `duck`, and others are allocated. A trait may not appear
twice. See [01_overview.md](../ducks/01_overview.md).

## Member types

```c++
using allocator_type = /* the allocator option */;
```

The [`allocator` option](options.md), `std::allocator<std::byte>` by default.

## Constructors

```c++
template <typename T>
duck(T&& obj);                                                          // (1)

template <typename T>
explicit duck(std::allocator_arg_t, const allocator_type& alloc, T&& obj); // (2)

template <typename T, typename... Args>
explicit duck(std::in_place_type_t<T>, Args&&... args);                 // (3)

template <typename T, typename... Args>
explicit duck(std::allocator_arg_t, const allocator_type& alloc,
              std::in_place_type_t<T>, Args&&... args);                 // (4)

template <typename T, typename U, typename... Args>
explicit duck(std::in_place_type_t<T>, std::initializer_list<U> il,
              Args&&... args);                                          // (5)

template <typename T, typename U, typename... Args>
explicit duck(std::allocator_arg_t, const allocator_type& alloc,
              std::in_place_type_t<T>, std::initializer_list<U> il,
              Args&&... args);                                          // (6)

template <typename Duck>
duck(Duck&& d);                                                         // (7)

template <typename Duck>
explicit duck(std::allocator_arg_t, const allocator_type& alloc, Duck&& d); // (8)

duck(const duck& other);                                                // (9)

duck(std::allocator_arg_t, const allocator_type& alloc, const duck& other); // (10)

duck(duck&& other);                                                     // (11)

duck(std::allocator_arg_t, const allocator_type& alloc, duck&& other);  // (12)
```

1. Stores a decayed copy of `obj`. Deleted if `T` doesn't satisfy `Traits...`.
2. (1), using `alloc`.
3. Constructs a `T` from `args...` in place. Deleted if `T` doesn't satisfy
   `Traits...`.
4. (3), using `alloc`.
5. Constructs a `T` from `il, args...` in place. Deleted if `T` doesn't satisfy
   `Traits...`.
6. (5), using `alloc`.
7. Takes over the object of a `duck` whose traits are a permutation of
   `Traits...`.
8. (7), using `alloc`.
9. Copies `other`'s object. Only available with [`copyable`](traits.md#copyable).
10. (9), using `alloc`.
11. Moves `other`'s object, leaving `other` valueless.
12. (11), using `alloc`.

(1), (3), (5) and (7) require `allocator_type` to be default-constructible.
`duck` has no default constructor.

## Destructor

```c++
~duck();
```

Destroys the object and frees its memory. Does nothing if the `duck` is valueless.

## Assignment

```c++
template <typename T>
duck& operator=(T&& obj);                                               // (1)

duck& operator=(const duck& other);                                     // (2)

duck& operator=(duck&& other);                                          // (3)
```

1. Replaces the object with a decayed copy of `obj`. Deleted if `T` doesn't
   satisfy `Traits...`.
2. Replaces the object with a copy of `other`'s. Only available with
   [`copyable`](traits.md#copyable).
3. Replaces the object with `other`'s, leaving `other` valueless.

Allocators propagate as `std::allocator_traits` says.

## Member functions

```c++
/* each member function and operator of Traits..., with the same signature */
```

Calls the object's matching function. Calling one on a valueless `duck` is
undefined behaviour, and asserts in debug builds.

## Non-member functions

### `make_narrowed`

```c++
template <typename... NewTraits, typename Duck>
duck<NewTraits...> make_narrowed(Duck&& src);                           // (1)

template <typename... NewTraits, typename Duck>
duck<NewTraits...> make_narrowed(std::allocator_arg_t,
                                 const typename duck<NewTraits...>::allocator_type& alloc,
                                 Duck&& src);                           // (2)
```

1. Copies or moves `src`'s object into a `duck` of a subset of its traits. See
   [03_trait_narrowing.md](../ducks/03_trait_narrowing.md).
2. (1), using `alloc`.

### `swap`

```c++
template <typename... Traits>
void swap(duck<Traits...>& lhs, duck<Traits...>& rhs);
```

Swaps the objects of `lhs` and `rhs`.

### `is_valueless`

```c++
template <typename... Traits>
bool is_valueless(const duck<Traits...>& d) noexcept;
```

Whether `d` holds no object, e.g. after being moved from.

### `emplace`

```c++
template <typename T, typename Duck, typename... Args>
T& emplace(Duck&& d, Args&&... args);                                   // (1)

template <typename T, typename U, typename Duck, typename... Args>
T& emplace(Duck&& d, std::initializer_list<U> il, Args&&... args);      // (2)
```

1. Replaces `d`'s object with a `T` constructed from `args...`, and returns it.
   `d` must be a non-`const` `duck`.
2. (1), constructing from `il, args...`.

### `get_allocator`

```c++
template <typename Duck>
typename std::decay_t<Duck>::allocator_type get_allocator(const Duck& d) noexcept;
```

Returns a copy of `d`'s allocator.

### `get`, `get_if`, `typeid_of`

See [access.md](access.md).

## Deduction guides

```c++
template <typename T>
duck(T&&) -> duck<>;
```
