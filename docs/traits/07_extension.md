# Extending Existing Types

Consider the following example:


Suppose you are using third-party code or the standard library and wish
to be able to extend their functionality to support a trait, like in the example below:

```c++
#include <other/library.hpp>

struct [[=rjk::trait]] Fooable {
    auto foo() -> int;
};

static_assert(rjk::satisfies<A, Fooable>); // Fails
```
```c++
// other/library.hpp

struct A {
    int data{};
};
```

Using the `rjk::duck` library, we can do this:

```c++
template <>
struct rjk::impl<A, Fooable> {
    static foo(const auto& self) {
        return self.data * 5;
    }
};

static_assert(rjk::satisfies<A, Fooable>); // Passes
```

`rjk::impl` allows extending any type to support the member functions
specified by any trait.

> [!NOTE]
> `rjk::impl` does not allow extension of operators; instead, use friend
> functions.