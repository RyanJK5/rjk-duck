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

> [!WARNING]
> Currently, in order to support operators, `rjk::duck` reserves
> any name defined in a trait that is prefixed with `_rjk_`. For example,
> defining a trait with a member function called `_rjk_meow` is ill-formed.

## Extensions

Types that are extended to support traits via `rjk::impl` (see [extension.md](../traits/07_extension.md))
can be called through a duck as though the member function were regularly defined.

```c++
struct [[=rjk::trait]] Fooable {
    auto foo() const -> int;
};

struct A {
    int data{};
};

template <>
struct rjk::impl<A, Fooable> {
    static auto foo(const auto& self) -> int {
        return self.data * 2;
    }
};

A a{35};
rjk::duck_view<Fooable> view{a};
assert(view.foo() == 70);
```

## Free Functions

The free functions `get` and `get_if` are provided in the `rjk` namespace
and behave very similarly to `std::variant`'s `get` and `get_if`.

```c++
struct [[=rjk::trait]] Trait {
    auto foo() -> void;
};

struct A { auto foo() -> void; };
struct B { auto foo() -> void; };

rjk::duck<Trait> myDuck{A{}};

A& obj = rjk::get<A>(myDuck);
int& obj2 = rjk::get<int>(myDuck); // ERROR - int does not satisfy Trait

// With -fno-exceptions, this is an assertion instead
try {
    B& obj = rjk::get<B>(myDuck);
} catch (const rjk::bad_duck_access& e) {
    std::println("{}", e.what()); // duck does not hold 'B'
}

A* obj = rjk::get_if<A>(&myDuck);
obj->foo(); // OK

// nullptr returned on failure
if (B* obj = rjk::get_if<B>(&myDuck)) {
    // never runs
    obj->foo();
}
```

## In-place Construction

Objects can be constructed in-place in `rjk::duck`, avoiding a potentially
costly move if needed. `duck` follows the pattern of standard library types
with similar features. To avoid potential naming conflicts with user-defined member functions from
traits, `emplace` is provided as a free function.

```c++
// Construct an object in-place with the provided arguments
rjk::duck<MyTrait> myDuck{std::in_place_type<MyObject>, ...};

// Destroys MyObject and constructs OtherObject in-place
rjk::emplace<OtherObject>(myDuck, ...);
```