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