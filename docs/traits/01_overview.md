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