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
x->size();   // 3
x->clear();
x->empty();  // true

x = std::string{"hello"};  // swap underlying type at runtime
x->size();   // 5
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
*x - 5;   // 5
5 - *x;   // -5
```

Operators are explicit: `*x` gives you the interface surface, `x` is the owning container.

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