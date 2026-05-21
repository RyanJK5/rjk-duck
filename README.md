# rjk::duck

A header-only C++26 type-erasure library powered by reflection. Define your interface once and don't worry about it again.


## Quick Start

**Requirements**:
- **C++26 compiler** with reflection support (gcc-trunk with `-freflection`)

### Type-erasing stdlib containers

```cpp
#include <duck.hpp>

using Sizeable = rjk::duck<
    rjk::has_fn<"size",  std::size_t() const>,
    rjk::has_fn<"clear", void()>,
    rjk::has_fn<"empty", bool() const>
>;

// std::vector, std::deque, std::string, std::map
Sizeable x{std::vector<int>{1, 2, 3}};
x.size();   // 3
x.clear();
x.empty();  // true

x = std::string{"hello"};  // swap underlying type at runtime
x.size();   // 5
```

No inheritance required. `std::vector` and `std::string` never heard of `Sizeable`.

### Operator support

```cpp
using Addable = rjk::duck<
    rjk::has_op<rjk::op_minus, int(const rjk::self&, int)>,
    rjk::has_op<rjk::op_minus, int(int, const rjk::self&)>
>;

struct Meter {
    int value;
    int operator-(int rhs) const { return value - rhs; }
};

int operator+(int lhs, const Meter& rhs) { return lhs - rhs.value; }

Addable x{Meter{10}};
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
ctest --test-dir build --parallel
```

## Future Plans

`rjk::duck` is presently aiming to satisfy this checklist before release 1.0:
- [x] Basic `rjk::has_fn` support for defining abstract interfaces with any `const`-ness or reference-qualification.
- [x] Basic `rjk::has_op` support for 1-2 operators (`+`/`-`) as proof-of-concept, including `rjk::self`.
- [ ] Code generation to support all C++ operators.
- [ ] Support for overload sets, allowing two `rjk::has_fn` with the same name different parameters.
- [ ] `rjk::self` support for return values to allow self-referential, implicit return operations
- [ ] `rjk::duck_view`, which offers a non-owning view into a duck.
- [ ] Basic `rjk::has_member` support, which allows for type-erased data members.
- [ ] `rjk::like`, which accepts a type and models the `duck` based on its public interface.
- [ ] `rjk::satisfies`, which accepts a concept and enforces that anything passed into the `duck` meets the constraints.
- [ ] Meta-concepts for `rjk::satisfies`, such as `any_of` and `all_of`.
- [ ] Readable error messages for incorrect duck usage.
- [ ] Automated production of a single header for the codebase.


Goals may change during the development process. Contribution is welcome.