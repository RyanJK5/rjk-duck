# Trait Narrowing

Consider two traits that are often stored on a `duck` together:

```c++
struct [[=rjk::trait]] Serializable {
    auto Serialize() const -> std::string;
};

struct [[=rjk::trait]] Deserializable {
    auto Deserialize(std::string& input) -> void;
};
```

You might store an `rjk::duck<Serializable, Deserializable>` for some types.
Now, in your `WriteFile` function, you only care if the object is `Serializable`:

```c++
auto WriteFile(const std::filesystem::path& file, 
               rjk::duck_view<const Serializable> obj) -> void {
    // open stream, write some metadata...
    ostr << obj.Serialize();
    // ...
} 
```

**Trait Narrowing** allows you to call `WriteFile` with a `duck<Serializable, ...>`.

```c++
rjk::duck<Serializable, Deserializable> myDuck{...};
WriteFile(somePath, myDuck);
```

They are two axes of narrowing: single-trait narrowing and `const` narrowing.

## Single-Trait Narrowing

A duck of any number of traits can be narrowed to a `duck_view` containing
only one of those traits. For example, a `duck<A, B, C>` can be narrowed to
a `duck_view<A>`, `duck_view<B>`, or `duck_view<C>` implicitly through `duck_view`'s
constructors. 

It is also possible to construct a `duck<A>` from a `duck<A, B, C>`,
but since this operation could potentially be costly (require a copy or move), it is
provided as an explicit free function called `make_narrowed`:

```c++
rjk::duck<A, B, C> myDuck{...};
rjk::duck<A> = rjk::make_narrowed<A>(myDuck);
```

## `const` Narrowing

A `duck<A, B, C>` can be narrowed to a totally-`const` version of that `duck`,
which is spelled `duck<const A, const B, const C>`. See 
[const_traits.md](../traits/02_const_traits.md) for an overview of how this
restricts a `duck`'s capabilities.

A `duck<A, B, C>` can also be narrowed to a `duck<const A>`, `duck<const B>`,
or `duck<const C>`. This can be thought of as a single-trait narrowing followed
by a `const` narrowing.

Both of these narrowing forms can be done implicitly for `duck_view` and through
`make_narrowed` for `duck`.

## Multi-Trait Narrowing

You may, for example, want to narrow a `duck<A, B, C>` to a `duck<A, B>`, or a
`duck<A, C>`, or a `duck<const A, B, const C>`. This introduces some
significant implementation complexity, and poses a large risk for compile times
and binary size, since a trait must provide conversions for the powerset of its
traits, which can quickly balloon for large numbers of traits.

This is an area that we hope to investigate going forward, and are considering
providing as a toggle-able feature.