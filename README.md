# rjk::duck

A header-only C++26 type-erasure library powered by reflection. Define your interface once and don't worry about it again.


## Quick Start

**Requirements**:
- **C++26 compiler** with reflection support (gcc-trunk with `-freflection`)

duck is header-only, so just copy the include directory and add it to your build system.

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
Traits can be composed naturally in the template argument lists of `duck` and `duck_view`. Trait subsumption lets you
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
- [X] `rjk::duck_view`, which offers a non-owning view into a duck.
- [X] Trait subsumption, allowing a `duck_view<TraitA>` to be made from a `duck<TraitA, TraitB>`.
- [X] `rjk::duck_ptr`, with is a nullable variant of `duck_view`.
- [X] Return type deduction for `duck`, `duck_view`, and `duck_ptr`.
- [X] `rjk::like`, which accepts a type and models the `duck` based on its public interface.
- [X] `rjk::impl`, which allows users to extend existing types to support traits or policies.
- [X] `constexpr` support for `duck`, `duck_view`, and `duck_ptr`.
- [X] SBO customization with the `[[=rjk::perf_options]]` annotation.
- [X] Readable error messages for incorrect duck usage.
- [X] Automated production of a single header for the codebase.


Goals may change during the development process. Contribution is welcome.
