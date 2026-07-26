# Trait Composition

Oftentimes, a type can satisfy multiple traits. There are two primary ways to
check this in `rjk::duck`.

## Inline Composition

`satisfies` is actually a variadic template that can accept more than one trait.

```c++
struct TraitA {
    auto foo() -> int;
    auto foo() const -> int;
};

struct TraitB {
    auto bar() -> std::string_view;
};

struct DemoOne {
    auto foo() -> int {
        return 20;
    }
    
    auto foo() const -> int {
        return 50;
    }
    
    auto bar() -> std::string_view {
        return "Hello, world!";
    }
};

struct DemoTwo {
    auto foo() const -> int {
        return 10;
    }
    
    auto bar() -> std::string_view {
        return "Something else";
    }
};

// DemoOne satisfies the entire interface of TraitA and TraitB
static_assert(rjk::satisfies<DemoOne, TraitA, TraitB>);

// DemoTwo satisfies only the const interface of TraitA
static_assert(!rjk::satisfies<DemoTwo, TraitA, TraitB>);
static_assert(rjk::satisfies<DemoTwo, const TraitA, TraitB>); 
```

Note from the example above that the `const`-ness of traits is independent
of each other. It is perfectly valid to require that a type satisfy the `const`
interface of `TraitA`, but the entire interface of `TraitB`.

The example also naturally extends to 3+ traits:

```c++
static_assert(rjk::satisfies<DemoOne, TraitA, TraitB, rjk::copyable>);
```

## Composition via Inheritance

For traits that are frequently bundled together, they can also be
composed via inheritance.

```c++
struct Reader {
    auto read(const std::filesystem::path& file) -> Data;
};

struct Writer {
    auto write(const Data& d, const std::filesystem::path& file) -> void;
};

struct ReadWriter : Reader, Writer { };
```

Traits can also inherit from policies:

```c++
struct TraitB : TraitA, rjk::copyable { ... };
```

## Handling Naming Conflcts

In general, any ambiguity between function overloads will result in a hard compile-time error.

```c++
// ERROR: read() appears twice
static_assert(rjk::satisfies<MyType, Reader, ReadWriter>);
```

The above example can trivially be avoided by just passing `ReadWriter` in instead of
`Reader` and `ReadWriter`. 