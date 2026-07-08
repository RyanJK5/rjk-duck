# Performance Tuning

The performance of `duck` is critical and can vary significantly based on the types
being passed in and what functions are being called on it the most. `duck` provides
various levers for controlling performance, but first, it is helpful to have a basic
understanding of how `duck` works internally.

## Under the Hood

Fundamentally, `duck` generates a static vtable that points to all
the functions you require via traits.

For example, the following traits:

```c++
struct [[=rjk::trait]] TraitA {
    auto foo() const -> int;
    auto bar(std::string_view) -> void;
};

struct [[=rjk::trait]] TraitB {
    auto doSmth() noexcept -> void;
};
```

Will (roughly) generate a vtable like this for `duck<TraitA, TraitB>`:

```c++
template <>
struct vtable<TraitA, TraitB> {
    // narrowing pointers
    const vtable<TraitA>* to_trait_0;
    const vtable<TraitB>* to_trait_1;
    const vtable<const TraitA, const TraitB>* to_const;
    
    // lifecycle functions
    auto (*move)(void*, void*) noexcept -> void;
    auto (*destroy)(void*) noexcept -> void;
    // copy is added with rjk::copyable
    
    // TraitA functions
    auto (*foo)(const void*) -> int;
    auto (*bar)(void*, std::string_view) -> void;
    
    // TraitB functions
    auto (*doSmth)(void*) noexcept -> void;
};
```

Then, when you instantiate a `duck` with some type `T`, it will create a
compile-time vtable for `T` and assign an internal pointer in `duck` to that
vtable.

In other words, `duck` looks something like this:

```c++
template <is_trait... Traits>
class duck {
  public:
    // ...
  private:
    void* m_underlying;
    const vtable<Traits...>* m_vtable;
};
```

Calling `myDuck.foo()` will dereference the `m_vtable` pointer, then call
the appropriate function pointer.

### Small Buffer Optimization

Small objects may be stored directly inside of `duck` to avoid a heap allocation.
The above structure of `duck` is therefore not fully accurate: 

```c++
template <is_trait... Traits>
class duck {
  public:
    // ...
  private:
    void* m_underlying;
    const vtable<Traits...>* m_vtable;
    
    alignas(sbo_align) std::array<std::byte, sbo_size> m_buffer;
};
```

Any object whose alignment and size are less than or equal to `sbo_align`
and `sbo_size` can fit inside the buffer. They must also be nothrow move
constructible.

## The `perf_options` Trait

By default, `sbo_size` is the size of two pointers (typically 16 bytes)
and `sbo_align` is `alignof(std::max_align_t)`. In some cases, you may want
to change these to store larger types without allocation, or to support types
with custom alignments. This can be accomplished by using the `[[=rjk::perf_options]]`
annotation to define a trait.

```c++
struct [[=rjk::perf_options]] MyOpts {
    std::size_t sbo_size = 32; // Larger size
    std::size_t sbo_alignment = 64; // Larger alignment
};

rjk::duck<MyOpts, ...> d{...};
```

A useful pattern is to tie the performance options to a trait via inheritance,
so as not to leak implementation details throughout code.

```c++
struct [[=rjk::trait]] MyTrait : MyOpts {
    ...
};

rjk::duck<MyTrait> d{...}; // Uses options from MyOpts
```

Do not that these will increase the size of your `duck`. It is recommended to
always measure the affects of your performance options before assuming they
caused improvements.

### Inlining vtable calls

As described above, calling `myDuck.foo()` requires two pointer dereferences,
which can be slow in many cases. To compensate, `perf_options` allows you to
inline specific function signatures.

```c++
struct [[=rjk::perf_options]] Inline {
    struct inlined_functions {
        auto foo() -> int;
    };
};

struct [[=rjk::trait]] Fooable : Inline {
    auto foo() -> int;
};
```

Now, calling `rjk::duck<Fooable>{...}.foo()` will result in a direct call to
the function pointer associated with `foo`. 

This comes with the tradeoff of increasing a `duck`'s size by one pointer 
(typically 8 bytes) for each additional inlined function. This optimization
is also applied to `duck_view` (i.e. calling `rjk::duck_view<Fooable>{...}.foo()` 
will also be a direct call), so be careful copying `duck_view`s with many inlined
functions. It may be more efficient to pass by reference instead.

For a mental model of how inlining functions looks under the hood, imagine this
as `rjk::duck<Fooable>`:

```c++
template <>
class duck<Fooable> {
  public:
    // ...
  private:
    void* m_underlying;
    
    const vtable<Fooable>* m_vtable; // for lifecycle, narrowing, etc
    auto (*foo)() -> int; // inlined call
    
    alignas(sbo_align) std::array<std::byte, sbo_size> m_buffer;
};
```

## Future Optimizations

The following additions are planned to be customization points for the 
`perf_options` trait:

- **Allocators**: Some form of support similar to either STL allocators
or PMR allocators will be added to `duck` and the `perf_options` trait.
The user-provided allocator will be used instead of default `new` when 
allocating an object outside of `duck`'s small buffer.
- **Variant Backend**: Oftentimes, it is possible to know the closed set
of types that may be passed into a `duck`. Adding the option for a
`std::variant`-like backend can result in switch-case dispatch rather
than function pointers, which provides performance gains in some situations.