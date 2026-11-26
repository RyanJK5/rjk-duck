# `duck_ptr`

```c++
template <typename... Traits>
class duck_ptr;
```

A [`duck_view`](duck_view.md) that may be empty. See
[02_duck_ptr.md](../ducks/02_duck_ptr.md).

## Constructors

```c++
duck_ptr() noexcept;                                                    // (1)

duck_ptr(std::nullopt_t) noexcept;                                      // (2)

duck_ptr(duck_view<Traits...> view) noexcept;                           // (3)

template <typename T>
duck_ptr(T&& obj) noexcept;                                             // (4)

duck_ptr(const duck_ptr& other) noexcept;                               // (5)
```

1. Empty.
2. Empty.
3. Holds `view`. Empty if `view` refers to a valueless `duck`.
4. Holds `duck_view<Traits...>{obj}`.
5. Holds the same view as `other`, or is empty if `other` is.

## Assignment

```c++
duck_ptr& operator=(const duck_ptr& other) noexcept;
```

Holds the same view as `other`, or becomes empty if `other` is.

## Member functions

```c++
bool has_value() const noexcept;                                        // (1)

explicit operator bool() const noexcept;                                // (2)

duck_view<Traits...> value() const;                                     // (3)

duck_view<Traits...> operator*() const noexcept;                        // (4)

duck_view<Traits...>* operator->() noexcept;                            // (5)

const duck_view<Traits...>* operator->() const noexcept;                // (5)

void reset() noexcept;                                                  // (6)
```

1. Whether it holds a view.
2. `has_value()`.
3. Returns the view. Throws [`bad_duck_access`](access.md#bad_duck_access) if
   empty, or asserts with `-fno-exceptions`.
4. Returns the view. Undefined behaviour if empty.
5. Returns a pointer to the view. Undefined behaviour if empty.
6. Makes it empty.

## Deduction guides

```c++
template <typename... Traits>
duck_ptr(duck<Traits...>&) -> duck_ptr<Traits...>;                      // (1)

template <typename... Traits>
duck_ptr(duck<Traits...>&&) -> duck_ptr<Traits...>;                     // (1)

template <typename... Traits>
duck_ptr(const duck<Traits...>&) -> duck_ptr<const Traits...>;          // (2)

template <typename... Traits>
duck_ptr(duck_view<Traits...>) -> duck_ptr<Traits...>;                  // (3)
```

1. A pointer to a `duck`'s traits.
2. A pointer to a `const` `duck`'s traits, all made `const`.
3. A pointer with a view's traits.
