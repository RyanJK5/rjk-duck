# `const` Traits

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

## Other Modifiers

Modifiers that could typically be applied to a function can also be applied to traits.

### Reference Qualification

Traits can be reference-qualified.

```c++
struct [[=rjk::trait]] Trait1 {
    int& foo() &;
};

struct [[=rjk::trait]] Trait2 {
    int&& bar() &&;
};

struct A {
    int x = 5; 
    int& foo() & { return x; }
};
struct B {
    int x = 10;
    
    int& foo() & { return x; }
    int&& bar() && { return x * 5; }
};

static_assert(rjk::satisfies<A, Trait1>);
static_assert(rjk::satisfies<B, Trait2>);
static_assert(!rjk::satisfies<B, Trait1>);
```

### Noexcept

A non-`noexcept` member of a trait can match any type with the function regardless of its `noexcept` guarantee. A
trait with a `noexcept` member will only match types that also mark the member function `noexcept`.

```c++
struct [[=rjk::trait]] WithoutNoexcept {
    int foo();
};

struct [[=rjk::trait]] WithNoexcept {
    int foo() noexcept;
};

struct A {
    int foo() noexcept { return 10; }
};

struct B {
    int foo() { return 20; }
};

static_assert(rjk::satisfies<A, WithoutNoexcept>);
static_assert(rjk::satisfies<B, WithoutNoexcept>);

static_assert(rjk::satisfies<A, WithNoexcept>);
static_assert(!rjk::satisfies<B, WithNoexcept>);
```