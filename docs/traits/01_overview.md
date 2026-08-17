# Traits

Traits are the fundamental building block of the duck library. Their primary purpose is
to specify the **interface** for ducks. They add functionality on top of the general member functions
that ducks have. 

## Basics

Traits are expected to contain *only* non-static member functions, which represent the interface. For example,
consider `MyTrait`:

```c++
struct MyTrait {
    auto foo() -> int;
    auto bar(bool someParam) -> int;
};
```

This defines an interface with member functions `foo` and `bar`. Note that the member
functions are only *declared*, not *defined*. Any definition for a member function in a trait will
be ignored.

> [!NOTE]
> Both trailing return and regular return syntax are accepted. For consistency,
> this library uses trailing return types.

According to the rules defined above, all of the following traits are ill-formed:

```c++
struct TraitA { // Doesn't contain annotation
    auto doSmth() -> void;
};

struct TraitB {
    static auto doSmth() -> void; // static function
};

struct TraitC {
    auto doSmth() -> void;
    
    int someMember; // non-member function
};
```

Traits entirely respect `const` and reference-qualification.

A type is considered to satisfy a trait if it defines all of the requisite member functions
declared in the trait's interface. This can be verified using the `rjk::satisfies` concept:

```c++
struct Fooable {
    auto foo() -> int;
};

struct MyStruct {
    auto foo() -> int { return 25; }
};

struct MyOtherStruct {
    auto foo(int input) -> int { return input * 2; }
};

static_assert(rjk::satisfies<MyStruct, Fooable>);
static_assert(!rjk::satisfies<MyOtherStruct, Fooable>);
```

## Copyability

If a trait has an **user-declared** default copy constructor,
it will require that all types passed in be copy constructible.

```c++
struct copyable {
    copyable(const copyable&) = default;
};

struct Struct1 {};

static_assert(rjk::satisfies<Struct1, copyable>);

struct Struct2 {
    Struct2(const Struct2&) = delete;
};
static_assert(!rjk::satisfies<Struct2, copyable>);

struct Struct3 {
    Struct3(const Struct3) = default;
    Struct3& operator=(const Struct3&) = delete;
};
// Struct3 is still copy constructible, so this is OK
static_assert(rjk::satisfies<Struct3, copyable>);
```

The `copyable` trait from above is provided by this library as `rjk::copyable` for your convenience.