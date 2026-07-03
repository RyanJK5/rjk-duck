# Ducks

`rjk::duck` and `rjk::duck_view` are the two primary class templates provided by this library. They both utilize type
erasure to allow their underlying type to be swapped at runtime, but also offer fully customizable compile-time
interfaces.

## Using Traits

Traits can be passed in the template parameters of a duck. The duck enforces that any type
passed into it satisfies those traits, then makes them available for use through member
functions and operators.

```c++
struct [[=rjk::trait]] TraitA {
    auto foo() -> int;
};

struct A {
    auto foo() -> int { return 10; }
};

struct B {
    auto foo() -> int { return 20; }
};

A a{};
rjk::duck<TraitA> d{a};
assert(d.foo() == 10);

d = B{}; // swap underlying type at runtime
assert(d.foo() == 20);

struct C {};
d = C{}; // ERROR: C does not satisfy TraitA
```

## `duck_view`

`rjk::duck_view` is simply a non-owning view into an object that satisfies the provided trait list. This may be a `duck`
or a concrete type. It can be implicitly constructed from any matching type.

Continuing with the example from above:

```c++
auto myFunc(rjk::duck_view<TraitA> f) -> int {
    return f.foo();
}

assert(myFunc(A{}) == 10;
    
rjk::duck<TraitA> d{B{}}; 
assert(myFunc(d) == 20);

rjk::duck_view<TraitA> view{B{}};
view.foo(); // ERROR: the temporary was automatically destroyed

B b{};
rjk::duck_view<TraitA> view2{b};
view.foo(); // OK
```

This example also demonstrates that duck_view is generally cheap to copy.

## Operators

Ducks are also fully compatible with operators.

```c++
struct [[=rjk::trait]] Callable {
    auto operator()(int input) const -> int;
};

auto myFunc(int input) -> int { return input * 3; }

struct A {
    auto operator()(int input) const -> int { return input * 2; }
};

rjk::duck<Callable> d{[](int input) {
    return input;
}};
assert(d(10) == 10);
d = A{};
assert(d(5) == 10);
d = myFunc;
assert(d(100) == 300);
```