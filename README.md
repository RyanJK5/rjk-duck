# rjk::duck

[![C++26](https://img.shields.io/badge/C%2B%2B-26-blue.svg)](https://en.cppreference.com/w/cpp/23)
[![License: Apache 2.0](https://img.shields.io/badge/License-Apache%202.0-yellow.svg)](LICENSE.txt)
![Platform: macOS | Linux](https://img.shields.io/badge/Platform-macOS%20%7C%20Linux%20-brightgreen.svg)

![Build Status](https://github.com/RyanJK5/rjk-duck/actions/workflows/build.yml/badge.svg)

A header-only C++26 type-erasure library powered by reflection. Define your interface once and don't worry about it again.


## Quick Start

**Requirements**:
- **C++26 compiler** with reflection support (gcc-trunk with `-freflection`)

duck is header-only, so just copy the include directory and add it to your build system.

Alternatively, it can be added to your CMake project using `FetchContent`, if you are using
**CMake 3.25+**:

```cmake
include(FetchContent)

FetchContent_Declare(
    rjk_duck
    GIT_REPOSITORY https://github.com/ryanjk5/rjk-duck.git
)
FetchContent_MakeAvailable(rjk::duck)
# ...
target_link_libraries(my_app PRIVATE rjk::duck)
```

### Basic Syntax

```cpp
#include <rjk/duck.hpp>
// ...

struct [[=rjk::trait]] Container {
    auto size()  const -> std::size_t;
    auto empty() const -> bool;
    auto clear()       -> void;
};

rjk::duck<Container> c{std::vector<int>{1, 2, 3}};
c.size();   // 3

c = std::string{"hello"};  // swap underlying type at runtime
c.size();   // 5

c = std::map<int, int>{{1, 2}, {3, 4}};
c.empty();  // false
c.clear();
c.empty();  // true
```

`Container` is a trait that specifies your type-erased interface. `duck` is an owning container that accepts traits 
and enforces that any types passed in meet the constraints at compile time. Then, the actual data it holds can be
swapped at runtime.

Already have inheritance-based interfaces in your code? Migrating them is as simple as one line:
```cpp
class IRenderer {
public:
    virtual ~IRenderer() = default;
    virtual auto BeginFrame() -> void = 0;
    virtual auto EndFrame()   -> void = 0;
    virtual auto DrawMesh(const Mesh& mesh, 
                          const Transform& transform) -> void = 0;
};

class VulkanRenderer { ... }; // doesn't inherit from IRenderer

using Renderer = rjk::like<IRenderer>;
rjk::duck<Renderer> d{VulkanRenderer{...}};
```
Now anything that acts like a Renderer can be a Renderer.

### duck_view

```cpp
struct [[=rjk::trait]] Drawable {
    auto draw(Canvas& canvas) const -> void;
    auto bounds() const             -> Rect;
};

void RenderAll(std::span<rjk::duck<Drawable>> shapes) {
    for (auto& shape : shapes) {
        shape.draw(canvas);
    }
}

void RenderHovered(rjk::duck_view<const Drawable> shape, Point cursor) {
    if (shape.bounds().contains(cursor)) {
        shape.draw(canvas);
    }
}

std::vector<rjk::duck<Drawable>> shapes;
shapes.emplace_back(Circle{...});
shapes.emplace_back(Rectangle{...});
shapes.emplace_back(Triangle{...});

RenderAll(shapes);

Circle myShape{...};
RenderHovered(myShape, {55, 55}); // also works with objects not stored in duck
```
`duck_view` is a view into anything that satisfies the traits. This could be an existing duck or any other valid type.
It's cheap to copy, offers both mutable and const semantics, and works together with `duck`.

### Multi-Trait Composition

```cpp
struct [[=rjk::trait]] Physical {
    auto ApplyForce(Vec2 force) -> void;
    auto velocity() const       -> Vec2;
};

struct [[=rjk::trait]] Renderable {
    auto Draw(Canvas& canvas) const -> void;
    auto bounds() const             -> Rect;
};

// Pass only what the function needs
void Update(rjk::duck_view<Physical> body) {
    body.ApplyForce(gravity);
}

void Render(rjk::duck_view<const Renderable> obj) {
    obj.Draw(canvas);
}


// A game object that is both physical and renderable
rjk::duck<Physical, Renderable> gameObject{PlayerEntity{}};
gameObject.ApplyForce({0, -9.8f});
gameObject.Draw(canvas);

Update(gameObject);   // implicit reduction from duck<Physical, Renderable>

std::vector<rjk::duck<Physical, Renderable>> gameObjects{};
gameObjects.push_back(std::move(gameObject));
gameObjects.push_back(...);

for (const auto& obj : gameObjects) {
    Render(obj);
}
```
Traits can be composed naturally in the template argument lists of `duck` and `duck_view`. Trait narrowing lets you
define the specific interface a view needs while still allowing the underlying `duck` to hold as many traits as it needs.

Anything that you want to bundle together can also be composed using inheritance:

```cpp
struct [[=rjk::trait]] GameObject : Physical, Renderable {};
```

### Extending Existing Classes

```cpp
struct [[=rjk::trait]] Serializable {
    auto Serialize() const -> std::string;
};

template <typename T> requires std::is_arithmetic_v<T>
struct rjk::impl<std::vector<T>, Serializable> {
    static auto Serialize(const auto& self) -> std::string {
        std::string out{};
        for (auto& member : self) {
           out += std::to_string(member);
           if (member != self.back()) {
               out += ",";
           }
        }
        return out;
    }
};

auto Serialize(std::span<const rjk::duck<Serializable>> data, 
               const std::filesystem::path& outFile) -> void {
    std::ofstream output{outFile};
    for (auto entry : data) {
        output << entry.Serialize() << '\n';
    }
}

std::vector<rjk::duck<Serializable>> data{...}; // other Serializable types
data.emplace_back(std::vector{1, 2, 3});
data.emplace_back(std::vector{4.0, 5.0, 6.0});
data.emplace_back(OtherSerializable{});

Serialize(data, std::filesystem::path{"myPath"});
```

### `constexpr` support

`duck` and `duck_view` are fully usable at compile-time.

```cpp

struct [[=rjk::trait]] Incrementable {
    auto operator+=(int value) -> rjk::duck_view<Incrementable>;
    auto operator++()          -> rjk::duck_view<Incrementable>;
    auto ToInt() const         -> int;
};

class Counter {
  public:
    constexpr Counter(float initial) : data(initial) { }
    
    constexpr auto operator+=(int value) -> Counter& {
        data += value;
        return *this;
    }
    
    constexpr auto operator++() -> Counter& {
        ++data;
        return *this;
    }
    
    constexpr auto ToInt() const -> int {
       return static_cast<int>(data);
    }
  private:
    float data;  
};

constexpr bool test_counter() {
    rjk::duck<Incrementable> d{Counter{10.0f}};
    d += 25;
    return (++d).ToInt() == 36;
} 

static_assert(test_counter());
```


### Run Tests

**Requirements**:
- **CMake 3.25+**
- **Ninja** or other CMake-compatible build system


```sh
# Configure
cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=Release -DRJK_BUILD_TESTS=ON

# Build
cmake --build build

# Test
ctest --test-dir build
```

## Future Plans

Contribution is welcome.

- [ ] Customizable lookup rules
- [ ] Allocator support (via `[[=rjk::perf_options]]`)
- [ ] `std::variant`-like backend support (via `[[=rjk::perf_options]]`)
- [ ] Support for function references in `rjk::duck_view`
- [ ] Multi-trait narrowing
- [ ] Free functions for inspecting the `typeid` underlying a `duck` (with `-fno-rtti` support)
- [ ] Module support


## License

This project is licensed under the terms of the [Apache 2.0 License](LICENSE.txt).