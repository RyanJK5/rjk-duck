# Accessing the Object

These need RTTI and [`rtti`](traits.md#rtti) in the duck's traits, and are
deleted otherwise. `Duck` is a `duck` or a `duck_view`, and `T` must satisfy its
traits. See [01_overview.md](../ducks/01_overview.md#free-functions).

## `get`

```c++
template <typename T, typename Duck>
decltype(auto) get(Duck&& d);
```

Returns a reference to `d`'s object as a `T`, with `d`'s value category. It is
`const` if `d` is a `const` duck, a view of `const` traits, or `T` is `const`.
Throws [`bad_duck_access`](#bad_duck_access) if `d` doesn't hold a `T`, or
asserts with `-fno-exceptions`.

## `get_if`

```c++
template <typename T, typename Duck>
auto* get_if(Duck* d) noexcept;
```

Returns a pointer to `d`'s object as a `T`, or `nullptr` if `d` doesn't hold a
`T`. It is `const` in the same cases as [`get`](#get).

## `typeid_of`

```c++
template <typename Duck>
const std::type_info& typeid_of(const Duck& d) noexcept;
```

Returns the `std::type_info` of `d`'s object, or `typeid(void)` if `d` is
valueless or a view of a valueless `duck`.

## `bad_duck_access`

```c++
class bad_duck_access final : public std::exception {
public:
    explicit bad_duck_access(const char* str);                         // (1)
    const char* what() const noexcept override;                         // (2)
};
```

Thrown by [`get`](#get) and [`duck_ptr::value`](duck_ptr.md#member-functions).

1. Stores `str`, which must outlive the exception.
2. Returns `str`.
