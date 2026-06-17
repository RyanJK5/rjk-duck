# Traits

Traits are the fundamental building block of the duck library. Their primary purpose is
to specify the **interface** for ducks. They add functionality on top of the general member functions
that ducks have. 

## Basics

A trait can be defined by creating a struct with the `rjk::trait` annotation. This is a new
concept in C++26, and is applied using the syntax `[[=rjk::trait]]`, after `struct` or `class`.


Traits are expected to contain *only* non-static member functions, which represent the interface. For example,
consider `MyTrait`:

```c++
struct [[=rjk::trait]] MyTrait {
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

struct [[=rjk::trait]] TraitB {
    static auto doSmth() -> void; // static function
};

struct [[=rjk::trait]] TraitC {
    auto doSmth() -> void;
    
    int someMember; // non-member function
};
```

Traits entirely respect `const` and reference-qualification.

A type is considered to satisfy a trait if it defines all of the requisite member functions
declared in the trait's interface. This can be verified using the `rjk::satisfies` concept:

```c++
struct [[=rjk::trait]] Fooable {
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

You can also check if a type is a valid trait using `is_trait`:

```c++
static_assert(rjk::is_trait<MyTrait>);
static_assert(!rjk::is_trait<int>);
```

## `const` Traits

Traits can contain `const` member functions:

```c++
struct [[=rjk::trait]] Holder {
    auto getData() const -> int;
    auto setData(int input) -> void;
};
```

They behave as you might expect:

```c++
struct Demo {
    int d{};
    
    auto getData() const -> int {
        return d;
    }
    
    auto setData(int input) -> void {
        d = input;
    }
};

// If Demo did *not* mark getData as const, this check would fail
static_assert(rjk::satisfies<Demo, Holder>);
```

Traits themselves can also be marked `const`, meaning they accept any type that at least
satisfies the `const` interface of the trait.

```c++
static_assert(rjk::satisfies<Demo, const Holder>);

class ReadOnlyDemo {
  public:
    ReadOnlyDemo(int data) : d(data) { }
    
    auto getData() const -> int { 
        return d; 
    }
  private:
    int d;
};

// Even though it does not define setData, ReadOnlyDemo still
// satisfies the const portion of Holder's interface.
static_assert(rjk::satisfies<ReadOnlyDemo, const Holder>);

static_assert(!rjk::satisfies<ReadOnlyDemo, Holder>);
```

## Operators

Traits can also enforce that a type defines certain operators. Here's a simple example with a unary operator:

```c++
struct [[=rjk::trait]] Negatable {
    auto operator-() -> int;
};

static_assert(rjk::satisfies<int, Negatable>);
```

It respects any type for which the operator is valid, not just those that define it as a member function.

For binary operators, there's a few ways to define them:

```c++
// Matches any type that can be used like 'x + 5'
struct [[=rjk::trait]] LhsAddable {
    auto operator+(int) const -> int;
};

// Matches any type that can be used like '5 - x'
struct [[=rjk::trait]] RhsSubtractable {
    [[=rjk::right_side]]
    auto operator-(int) const -> int;
};

// Matches any type that can be used like 'x / 5' AND '5 / x'.
// This does NOT require that the operator is commutative, just
// that both sides are defined.
struct [[=rjk::trait]] BothDividable {
    [[=rjk::both_sides]]
    auto operator/(int) const -> int;
};
```

the `right_side` annotation and `both_sides` annotations are useful for
matching against types that overload operators using friends. For example:

```c++
struct [[=rjk::trait]] Printable {
    [[=rjk::right_side]]
    auto operator<<(std::ostream& out) const -> std::ostream&;
};

struct Foo {
    friend auto operator<<(std::ostream& out, const Foo& foo) -> std::ostream& {
        out << "foo";
        return out;
    }
};

static_assert(rjk::satisfies<int, Printable>);
static_assert(rjk::satisfies<Foo, Printable>);
```

Note that `const`-ness is still checked here. If `Foo` declared `operator<<` with a 
non-`const` `Foo&` as a friend, it would fail to satisfy `Printable`.

## Policies

**Policies** are a type of trait defined using different syntax. Specifically, policies are
specified by using `rjk::policy` along with template arguments called **tags**.

In general, they can be used identically to traits defined using the `trait` annotation.

> [!IMPORTANT]
> In general, `struct`-style traits should be preferred to policies, with
> the exception of `copyable` (see below). Policies are available to make
> metaprogramming with `rjk::duck` more achievable, since it is currently
> impossible to generate `struct`-style traits using reflection.

### Basic Syntax

```c++
using MyTrait = rjk::policy<
    rjk::has_fn<"foo", auto() -> int>,
    rjk::has_fn<"bar", auto(int) const -> int>
>;
```

The policy above is equivalent to:

```c++
struct [[=rjk::trait]] MyTrait {
    auto foo() -> int;
    auto bar(int input) const -> int;
};
```

This demonstrates the `has_fn` tag, which simply requires that a member function is present
with the provided identifier and signature.

### Operators

Policies use the `has_op` tag to define operators. Rather than using annotations like for regular traits, policies define operator function signatures
using a dummy parameter called `self`.

```c++
using Addable = rjk::policy<
    rjk::has_op<rjk::op_plus, auto(const rjk::self&, int) -> int>,
    rjk::has_op<rjk::op_plus, auto(int, const rjk::self&) -> int>
>;

static_assert(rjk::satisfies<int, Addable>);
static_assert(rjk::satisfies<int, const Addable>);
```

The above is equivalent to:

```c++
struct [[=rjk::trait]] Addable {
    [[=rjk::both_sides]]
    auto operator+(int) const -> int;
};
```

Operators where the operand is the left hand side can also be defined using
member function syntax by omitting `self`:

```c++
using Negatable = rjk::policy<
    rjk::has_op<rjk::op_minus, int() const>
>;
```

Equivalent to:

```c++
struct [[=rjk::trait]] Negatable {
    auto operator-() const -> int;
};
```

### `copyable` and `copy_tag`

By default, traits make no assumption about the movability or copyability of the types they
are checked against. By using the `copyable` policy as a trait, you can require that a type
be copyable.

```c++
static_assert(rjk::is_trait<rjk::copyable>);

static_assert(rjk::satisfies<int, rjk::copyable>);
static_assert(!rjk::satisfies<std::unique_ptr<int>, rjk::copyable>);
```

There is currently no equivalent `rjk::movable`. The reasoning for this is expanded upon
in [duck.md](02_duck.md).

`copyable` internally is just an alias:

```c++
using copyable = policy<copy_tag>;
```

`copy_tag` can therefore be composed as a tag in policies. In general, though, 
the `rjk::copyable` policy covers most use cases.

## Using Existing Code with `like`

`like` is a special trait that can accept non-trait types and turn them into traits.

```c++
class IDrawable {
  public:
    virtual auto draw(Graphics& g) const -> void = 0;
};

class TypeA : public IDrawable { ... };

class TypeB {
  public:
    auto draw(Graphics& g) const -> void { ... }
};

using DrawableTrait = rjk::like<IDrawable>;

static_assert(rjk::satisfies<TypeA, DrawableTrait>);
static_assert(rjk::satisfies<TypeB, DrawableTrait>);
```

The first template argument to `like` is the type to model. It will take in all **public** member
functions of the type, and any member operator functions. Notably, it does *not* find free
function operators.

`like` will implicitly exclude member functions that are not user-declared and unnamed members
like constructors and destructors.

The second template parameter to `like` is optional but allows you to further control what
member functions to take from the type.

```c++
class Interface {
  public:
    virtual auto importantFunc(int arg) -> void = 0;
    
    auto unimportantFunc() const -> int { ... }
};

// Take in importantFunc, but NOT unimportantFunc.
using Trait = rjk::like<Interface, std::meta::is_virtual>;
```

The second template argument can be any predicate with the signature 
`auto(std::meta::info) -> bool`. The filter is then applied to all of the
type's member functions.

Some useful predicates are provided by the library:

```c++
// all_of: ANDs together several predicates
using TraitA = rjk::like<MyType, rjk::all_of<
    std::meta::is_virtual,
    std::meta::is_const
>>;

// any_of: ORs together several predicates
using TraitB = rjk::like<MyType, rjk::any_of<
    std::meta::is_noexcept,
    std::meta::is_const,
    std::meta::is_lvalue_reference_qualified
>>;

// none_of: ORs together predicates, then NOTs the result
using TraitC = rjk::like<MyType, rjk::none_of<
    std::meta::is_volatile,
    std::meta::is_override
>>;

// include / exclude member functions based on their name
using TraitD = rjk::like<MyType, rjk::exclude<
    "uselessFunc", "otherUselessFunc"
>>;

using TraitE = rjk::like<MyType, rjk::include<
    "importantFunc", "otherImportantFunc"
>>;
```

And of course, custom predicates can be used as well:

```c++
consteval auto validFunction(std::meta::info func) -> bool {
    return std::ranges::contains_subrange(identifier_of(func), "foo"sv);
}

using MyLike = rjk::like<SomeType, validFunction>;
```

## Trait Composition

Oftentimes, a type can satisfy multiple traits. There are two primary ways to 
check this in `rjk::duck`.

### Inline Composition

`satisfies` is actually a variadic template that can accept more than one trait.

```c++
struct [[=rjk::trait]] TraitA {
    auto foo() -> int;
    auto foo() const -> int;
};

struct [[=rjk::trait]] TraitB {
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

### Composition via Inheritance

For traits that are frequently bundled together, they can also be
composed via inheritance.

```c++
struct [[=rjk::trait]] Reader {
    auto read(const std::filesystem::path& file) -> Data;
};

struct [[=rjk::trait]] Writer {
    auto write(const Data& d, const std::filesystem::path& file) -> void;
};

struct [[=rjk::trait]] ReadWriter : Reader, Writer { };
```

Traits can also inherit from policies:

```c++
struct [[=rjk::trait]] TraitB : TraitA, rjk::copyable { ... };
```

### Handling Naming Conflcts

In general, any ambiguity between function overloads will result in a hard compile-time error.

```c++
// ERROR: read() appears twice
static_assert(rjk::satisfies<MyType, Reader, ReadWriter>);
```

The above example can trivially be avoided by just passing `ReadWriter` in instead of
`Reader` and `ReadWriter`. 

## Conclusion

While traits have some uses for compile-time verification, their primary purpose is to be used with
ducks. In [duck.md](02_duck.md), we'll cover `rjk::duck` and how to use traits with ducks.