# Duck Covariance

Consider a trait that requires a `clone` method:

```c++
struct [[=rjk::trait]] Clonable {
    auto clone() const -> rjk::duck<Clonable>;
};
```

The following object can satisfy `Clonable`:

```c++
struct Object {
    // ...
    
    auto clone() const -> Object {
        return ...;     
    }
};

static_assert(rjk::satisfies<Object, Clonable>);
```

Even though its `clone` method returns an `Object`, this code is valid
because `Object` satisfies `Clonable`. There are three cases where **duck covariance**
is allowed.

## Conversion to `duck`

A `rjk::duck<Traits...>` will match any function that returns a `T` or `T&&`
where `rjk::satisfies<T, Traits...>` is true.

Return-by-value and rvalue returns are allowed because they will result in a
move into the `duck` if possible. It will not match a `T&`, `const T&`, or `const T&&`
return as those will require a potentially expensive copy. If this behavior is
explicitly desired, consider specializing `impl` instead.

## Conversion to `duck_view`

A `rjk::duck_view<Traits...>` will match any function that returns a `T&`
where `rjk::satisfies<T, Traits...>` is true. A `rjk::duck_view<const Traits...>`
will work for a `const T&` return.

```c++
struct [[=rjk::trait]] Fooable {
    auto foo() -> int;
};

struct [[=rjk::trait]] FooIterator {
    auto operator*() const -> rjk::duck_view<Fooable>;
    auto operator++() -> rjk::duck_view<FooIterator>;
};

struct A {
    int data{};
    auto foo() const -> int { return data; }
};

std::vector foos{A{1}, A{2}, A{3}, A{4}};
rjk::duck<FooIterator> it{foos.begin()};
(*it).foo(); // 1
++it;
(*it).foo(); // 2
```

## Conversion to `duck_ptr`

A natural extension of the previous example would be to allow `operator->`
to match a `T*` or `const T*`. While a `duck_view` would make sense here,
it's possible for a pointer to be nullable, so `T*` and `const T*` are
covariantly convertible to a `duck_ptr<Traits...>` and
`duck_ptr<const Traits...>` respectively.

```c++
struct [[=rjk::trait]] FooIterator {
    // stuff from above...
    auto operator->() -> rjk::duck_ptr<Fooable>;
};

rjk::duck<FooIterator> it{foos.begin()};
it->foo(); // 1
```