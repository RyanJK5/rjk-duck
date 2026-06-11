# rjk::duck

A header-only C++26 type-erasure library powered by reflection. Define your interface once and don't worry about it again.


## Quick Start

**Requirements**:
- **C++26 compiler** with reflection support (gcc-trunk with `-freflection`)

### Type-erasing stdlib containers

```cpp
#include <duck.hpp>

struct [[=rjk::trait]] Sizeable {
    auto size()  const -> std::size_t;
    auto clear()       -> void;
    auto empty() const -> bool;
};

// std::vector, std::deque, std::string, std::map
rjk::duck<Sizeable> x{std::vector<int>{1, 2, 3}};
x.size();   // 3
x.clear();
x.empty();  // true

x = std::string{"hello"};  // swap underlying type at runtime
x.size();   // 5
```

No inheritance required. `std::vector` and `std::string` never heard of `Sizeable`.

### Operator support

```cpp
struct [[=rjk::trait]] Subtractable {
    [[=rjk::both_sides]]
    int operator-(int) const;
};

struct Meter {
    int value;
    int operator-(int rhs) const { return value - rhs; }
};

int operator-(int lhs, const Meter& rhs) { return lhs - rhs.value; }

rjk::duck<Subtractable> x{Meter{10}};
x - 5;   // 5
5 - x;   // -5
```

### Run Tests

**Requirements**:
- **CMake 3.25+**
- **Ninja** or other CMake-compatible build system


```sh
# Configure
cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build

# Test
ctest --test-dir build
```

## Future Plans

`rjk::duck` is presently aiming to satisfy this checklist before release 1.0:
- [x] Basic `rjk::has_fn` support for defining abstract interfaces with any `const`-ness or reference-qualification.
- [x] Basic `rjk::has_op` support for 1-2 operators (`+`/`-`) as proof-of-concept, including `rjk::self`.
- [x] Code generation to support all C++ operators.
- [x] Support for overload sets, allowing overloads of `rjk::has_fn` and `rjk::has_op`.
- [X] Modular trait API to compose interfaces.
- [X] Ability to use `const` traits and mix `const` and non-`const` traits.
- [X] Ability to compose traits and policies using inheritance.
- [X] Trait subsumption, allowing a `duck_view<TraitA>` to be made from a `duck<TraitA, TraitB>`.
- [ ] `rjk::self` support for return values to allow self-referential, implicit return operations.
- [X] `rjk::duck_view`, which offers a non-owning view into a duck.
- [ ] `rjk::like`, which accepts a type and models the `duck` based on its public interface.
- [ ] `rjk::impl`, which allows users to extend existing types to support traits or policies.
- [ ] Readable error messages for incorrect duck usage.
- [ ] Automated production of a single header for the codebase.
- [ ] `rjk::satisfies`, which accepts a concept and enforces that anything passed into the `duck` meets the constraints. (Dependent on concept-template parameters)


Goals may change during the development process. Contribution is welcome.
