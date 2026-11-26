# Performance Options

```c++
struct /* any trait */ {
    std::size_t sbo_size = 3 * sizeof(void*);                           // (1)
    std::size_t sbo_alignment = alignof(void*);                         // (2)
    using allocator = std::allocator<std::byte>;                        // (3)
};
```

Declared as members of any of a duck's traits, or of their bases, and shown
here with their defaults. Several traits may set the same option, but different
values are a compile error. See
[05_performance_tuning.md](../ducks/05_performance_tuning.md#performance-options).

1. The size of the buffer objects are stored in without allocating.
2. The alignment of that buffer.
3. The allocator for objects that don't fit the buffer, and for constructing
   those that do.

An object is stored in the buffer if it fits, is aligned to at most
`sbo_alignment`, and can be moved through the allocator without throwing.
