# `duck_ptr`

```c++
template <typename... Traits>
class duck_ptr;
```

A nullable [`duck_view`](duck_view.md). See
[02_duck_ptr.md](../ducks/02_duck_ptr.md).

## Constructors

```c++
duck_ptr() noexcept;                       // (1)
duck_ptr(std::nullopt_t) noexcept;         // (1)
duck_ptr(duck_view<Traits...> view) noexcept; // (2)
template <typename T>
duck_ptr(T&& obj) noexcept;                // (3)
```

1. Empty.
2. Holds `view`. Empty if `view` refers to a valueless `duck`.
3. Holds `duck_view<Traits...>{obj}`.

## Member functions

| Function | Description |
|---|---|
| `bool has_value() const noexcept` | Whether it holds a view. |
| `explicit operator bool() const noexcept` | `has_value()`. |
| `duck_view<Traits...> value() const` | The view. Throws `bad_duck_access` if empty, or asserts with `-fno-exceptions`. |
| `duck_view<Traits...> operator*() const noexcept` | The view. Undefined behaviour if empty. |
| `duck_view<Traits...>* operator->() noexcept` | Pointer to the view. Undefined behaviour if empty. Also `const`. |
| `void reset() noexcept` | Makes it empty. |

## Deduction guides

```c++
template <typename... Traits>
duck_ptr(duck<Traits...>&) -> duck_ptr<Traits...>;
template <typename... Traits>
duck_ptr(duck<Traits...>&&) -> duck_ptr<Traits...>;
template <typename... Traits>
duck_ptr(const duck<Traits...>&) -> duck_ptr<const Traits...>;
template <typename... Traits>
duck_ptr(duck_view<Traits...>) -> duck_ptr<Traits...>;
```
