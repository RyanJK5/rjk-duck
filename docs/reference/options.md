# Performance Options

Declared as members of any of a duck's traits, or of their bases. Several
traits may set the same option, but different values are a compile error. See
[05_performance_tuning.md](../ducks/05_performance_tuning.md#performance-options).

| Member | Default | Description |
|---|---|---|
| `std::size_t sbo_size` | `3 * sizeof(void*)` | Size of the buffer objects are stored in without allocating. |
| `std::size_t sbo_alignment` | `alignof(void*)` | Alignment of that buffer. |
| `using allocator` | `std::allocator<std::byte>` | Allocator for objects that don't fit the buffer, and for constructing those that do. |

An object is stored in the buffer if it fits, is aligned to at most
`sbo_alignment`, and can be moved through the allocator without throwing.

```c++
struct BigBuffer {
    std::size_t sbo_size = 64;
};

rjk::duck<Trait, BigBuffer> d{...};
```
