#include "rjk/duck.hpp"

#include <gtest/gtest.h>

namespace rjk_test {

// Shared trait and concrete types

struct [[=rjk::trait]] Labeled {
    auto label() const -> std::string;
};

struct Cat {
    auto label() const -> std::string { return "cat"; }
};

struct Dog {
    auto label() const -> std::string { return "dog"; }
};

struct Elephant {
    std::array<std::byte, 64> padding{};
    auto label() const -> std::string { return "elephant"; }
};

using AnimalDuck = rjk::duck<Labeled>;

TEST(DuckConstruction, FromRvalue) {
    AnimalDuck d{Cat{}};
    EXPECT_EQ(d.label(), "cat");
}

TEST(DuckConstruction, FromLvalueCopiesTheObject) {
    Cat cat{};
    AnimalDuck d{cat};
    EXPECT_EQ(d.label(), "cat");
}

TEST(DuckConstruction, HeapAllocatedType) {
    AnimalDuck d{Elephant{}};
    EXPECT_EQ(d.label(), "elephant");
}

TEST(DuckConstruction, InPlaceTypeDefaultConstructs) {
    AnimalDuck d{std::in_place_type<Cat>};
    EXPECT_EQ(d.label(), "cat");
}

TEST(DuckConstruction, InPlaceTypeForwardsConstructorArgs) {
    struct Named {
        std::string name;

        explicit Named(std::string n) : name(std::move(n)) {}

        auto label() const -> std::string { return name; }
    };

    AnimalDuck d{std::in_place_type<Named>, "Rex"};
    EXPECT_EQ(d.label(), "Rex");
}

TEST(DuckConstruction, InPlaceTypeWithInitializerList) {
    struct FromList {
        int sum = 0;

        FromList(std::initializer_list<int> il) {
            for (int v : il) sum += v;
        }

        auto label() const -> std::string { return std::to_string(sum); }
    };

    AnimalDuck d{std::in_place_type<FromList>, {1, 2, 3}};
    EXPECT_EQ(d.label(), "6");
}

TEST(DuckAssignment, RvalueAssignmentReplacesTheUnderlyingObject) {
    AnimalDuck d{Cat{}};
    d = Dog{};
    EXPECT_EQ(d.label(), "dog");
}

TEST(DuckAssignment, LvalueAssignmentCopiesTheObject) {
    AnimalDuck d{Cat{}};
    Dog dog{};
    d = dog;
    EXPECT_EQ(d.label(), "dog");
}

TEST(DuckAssignment, AssigningAHeapAllocatedType) {
    AnimalDuck d{Cat{}};
    d = Elephant{};
    EXPECT_EQ(d.label(), "elephant");
}

TEST(DuckAssignment, RepeatedAssignmentSwapsBackAndForth) {
    AnimalDuck d{Cat{}};
    d = Dog{};
    EXPECT_EQ(d.label(), "dog");
    d = Elephant{};
    EXPECT_EQ(d.label(), "elephant");
    d = Cat{};
    EXPECT_EQ(d.label(), "cat");
}

struct NotLabeled {};
static_assert(!rjk::satisfies<NotLabeled, Labeled>);

}