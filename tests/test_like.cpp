#include <gtest/gtest.h>

#include "duck.hpp"
#include "duck_view.hpp"

namespace rjk_test {

class MyInterface {
public:
    virtual ~MyInterface() = default;

    virtual int foo() = 0;

    virtual int bar() = 0;

    virtual int buzz() {
        return 25;
    }
};

class TestA : public MyInterface {
    int foo() override { return 1; }
    int bar() override { return 2; }
};

struct TestB {
    int foo() { return 3; }
    int bar() { return 4; }
    int buzz() { return 30; }
};

TEST(LikeTrait, Basic) {
    rjk::duck<rjk::like<MyInterface>> d{TestA{}};
    EXPECT_EQ(d.foo(), 1);
    EXPECT_EQ(d.buzz(), 25);
    d = TestB{};
    EXPECT_EQ(d.bar(), 4);
    EXPECT_EQ(d.buzz(), 30);

    rjk::duck<rjk::like<MyInterface, std::meta::is_pure_virtual>> d2{TestA{}};
    EXPECT_EQ(d2.foo(), 1);
    // d2.buzz(); ERROR
}

TEST(LikeTrait, PredicateCombination) {
    using MyLike = rjk::like<MyInterface, rjk::all_of<
        std::meta::is_pure_virtual,
        rjk::exclude<"bar">
    >>;

    rjk::duck<MyLike> d{TestA{}};
    EXPECT_EQ(d.foo(), 1);
    // d.bar(); // ERROR
    // d.buzz(); // ERROR
}

class Drawable {
public:
    virtual ~Drawable() = default;
    virtual void draw() const = 0;
    virtual void set_color(int r, int g, int b) = 0;
    std::string name() const { return "Drawable"; }
};

class Resizable {
public:
    virtual ~Resizable() = default;
    virtual void resize(float factor) = 0;
    virtual float area() const = 0;
};

struct Circle {
    int r{}, g{}, b{};
    float radius{1.0f};

    void draw() const { /* ... */ }
    void set_color(int r, int g, int b) { this->r = r; this->g = g; this->b = b; }
    std::string name() const { return "Circle"; }
    void resize(float factor) { radius *= factor; }
    float area() const { return 3.14159f * radius * radius; }
};

struct Square {
    float side{1.0f};
    int r{}, g{}, b{};

    void draw() const { /* ... */ }
    void set_color(int r, int g, int b) { this->r = r; this->g = g; this->b = b; }
    void resize(float factor) { side *= factor; }
    float area() const { return side * side; }
};

// const like restricts to const-qualified methods only
TEST(LikeTrait, ConstLike) {
    Circle c{};
    c.radius = 2.0f;
    rjk::duck_view<const rjk::like<Drawable>> view{c};
    view.draw(); // fine, const method
    // view.set_color(1, 2, 3); // ERROR - mutable method
}

// like with pure_virtual brings in only pure virtual methods
// allowing non-inheriting types to satisfy the trait
TEST(LikeTrait, PureVirtualNonInheriting) {
    using DrawTrait = rjk::like<Drawable, std::meta::is_pure_virtual>;

    struct FreeDrawable {
        void draw() const {}
        void set_color(int, int, int) {}
        // no name(), no inheritance from Drawable
    };

    rjk::duck<DrawTrait> d{FreeDrawable{}};
    d.draw();
    d.set_color(255, 0, 0);
}

// Multi-trait duck with like traits, single-trait subsumption
TEST(LikeTrait, SingleTraitSubsumption) {
    using DrawTrait = rjk::like<Drawable, std::meta::is_pure_virtual>;
    using ResizeTrait = rjk::like<Resizable, std::meta::is_pure_virtual>;

    rjk::duck<DrawTrait, ResizeTrait> d{Circle{}};
    d.resize(2.0f);
    d.draw();

    rjk::duck_view<DrawTrait> draw_view{d};
    draw_view.draw();
    // draw_view.resize(2.0f); // ERROR

    rjk::duck_view<const DrawTrait> const_draw_view{d};
    const_draw_view.draw(); // fine
    // const_draw_view.set_color(1, 2, 3); // ERROR
}

// duck_ptr as nullable duck_view
TEST(LikeTrait, DuckPtr) {
    using DrawTrait = rjk::like<Drawable, std::meta::is_pure_virtual>;

    rjk::duck<DrawTrait> d{Circle{}};
    rjk::duck_ptr<DrawTrait> ptr{d};
    ASSERT_TRUE(ptr.has_value());
    ptr->draw();

    rjk::duck_ptr<DrawTrait> null_ptr{};
    ASSERT_FALSE(null_ptr.has_value());
}

// policy trait as anonymous inline trait
TEST(LikeTrait, PolicyTrait) {
    using AreaTrait = rjk::policy<
        rjk::has_fn<"area", float() const>,
        rjk::has_fn<"resize", void(float)>
    >;

    rjk::duck<AreaTrait> d{Circle{}};
    EXPECT_NEAR(d.area(), 3.14159f, 0.001f);
    d.resize(2.0f);
    EXPECT_NEAR(d.area(), 12.566f, 0.01f);

    d = Square{};
    EXPECT_NEAR(d.area(), 1.0f, 0.001f);
}

// Combining like with policy via multi-trait duck
TEST(LikeTrait, LikeAndPolicy) {
    using DrawTrait = rjk::like<Drawable, std::meta::is_pure_virtual>;
    using NameTrait = rjk::policy<rjk::has_fn<"name", std::string() const>>;

    rjk::duck<DrawTrait, NameTrait> d{Circle{}};
    d.draw();
    EXPECT_EQ(d.name(), "Circle");

    // Single-trait subsumption to NameTrait view
    rjk::duck_view<NameTrait> name_view{d};
    EXPECT_EQ(name_view.name(), "Circle");
}

// any_of predicate brings in methods matching any condition
TEST(LikeTrait, AnyOfPredicate) {
    using MixedTrait = rjk::like<Drawable,
        rjk::any_of<std::meta::is_pure_virtual, rjk::include<"name">>
    >;

    rjk::duck<MixedTrait> d{Circle{}};
    d.draw();
    d.set_color(1, 2, 3);
    EXPECT_EQ(d.name(), "Circle"); // included via name predicate even though non-virtual
}

// none_of excludes methods matching any condition
TEST(LikeTrait, NoneOfPredicate) {
    using NonPureTrait = rjk::like<Drawable,
        rjk::none_of<std::meta::is_pure_virtual>
    >;

    rjk::duck<NonPureTrait> d{Circle{}};
    EXPECT_EQ(d.name(), "Circle"); // non-virtual, included
    // d.draw(); // ERROR - pure virtual, excluded
}

}