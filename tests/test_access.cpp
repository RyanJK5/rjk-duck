#include "rjk/duck.hpp"

#include <gtest/gtest.h>

namespace rjk_test {

// ============================================================
// Shared trait and concrete types
// ============================================================

struct [[=rjk::trait]] Named {
    auto name() const -> std::string;
};

struct Alpha {
    auto name() const -> std::string { return "alpha"; }
};

struct Beta {
    auto name() const -> std::string { return "beta"; }
};

struct [[=rjk::perf_options]] NamedPerf {
    std::size_t sbo_size = 0;
};

using NamedDuck = rjk::duck<Named>;

TEST(DuckEmplace, ConstructsTheNewObjectInPlace) {
    NamedDuck d{Alpha{}};
    rjk::emplace<Beta>(d);
    EXPECT_EQ(d.name(), "beta");
}

TEST(DuckEmplace, ReturnsAReferenceToTheNewObject) {
    NamedDuck d{Alpha{}};
    auto& ref = rjk::emplace<Beta>(d);
    EXPECT_EQ(ref.name(), "beta");
}

TEST(DuckEmplace, ForwardsConstructorArguments) {
    struct Named2 {
        std::string label;

        explicit Named2(std::string s) : label(std::move(s)) {}

        auto name() const -> std::string { return label; }
    };

    NamedDuck d{Alpha{}};
    rjk::emplace<Named2>(d, "custom");
    EXPECT_EQ(d.name(), "custom");
}

TEST(DuckEmplace, ForwardsInitializerList) {
    struct FromList {
        int sum = 0;

        FromList(std::initializer_list<int> il) {
            for (int v : il) sum += v;
        }

        auto name() const -> std::string { return std::to_string(sum); }
    };

    NamedDuck d{Alpha{}};
    rjk::emplace<FromList>(d, {10, 20});
    EXPECT_EQ(d.name(), "30");
}

TEST(DuckGet, LvalueSucceedsForTheHeldType) {
    NamedDuck x{Alpha{}};
    EXPECT_NO_THROW(rjk::get<Alpha>(x));
}

TEST(DuckGet, ConstLvalueSucceedsForTheHeldType) {
    const NamedDuck x{Alpha{}};
    EXPECT_NO_THROW(rjk::get<Alpha>(x));
}

TEST(DuckGet, RvalueSucceedsForTheHeldType) {
    NamedDuck x{Alpha{}};
    EXPECT_NO_THROW(rjk::get<Alpha>(std::move(x)));
}

TEST(DuckGet, ConstRvalueSucceedsForTheHeldType) {
    const NamedDuck x{Alpha{}};
    EXPECT_NO_THROW(rjk::get<Alpha>(std::move(x)));
}

TEST(DuckGet, LvalueThrowsOnTypeMismatch) {
    NamedDuck x{Alpha{}};
    EXPECT_THROW(rjk::get<Beta>(x), rjk::bad_duck_access);
}

TEST(DuckGet, RvalueThrowsOnTypeMismatch) {
    NamedDuck x{Alpha{}};
    EXPECT_THROW(rjk::get<Beta>(std::move(x)), rjk::bad_duck_access);
}

TEST(DuckGet, ConstRvalueThrowsOnTypeMismatch) {
    const NamedDuck x{Alpha{}};
    EXPECT_THROW(rjk::get<Beta>(std::move(x)), rjk::bad_duck_access);
}

TEST(DuckGetIf, ReturnsNonNullForTheHeldType) {
    NamedDuck x{Alpha{}};
    EXPECT_NE(rjk::get_if<Alpha>(&x), nullptr);
}

TEST(DuckGetIf, ReturnsNullOnTypeMismatch) {
    NamedDuck x{Alpha{}};
    EXPECT_EQ(rjk::get_if<Beta>(&x), nullptr);
}

TEST(DuckGetIf, RespectsConstness) {
    const NamedDuck x{Alpha{}};
    EXPECT_NE(rjk::get_if<Alpha>(&x), nullptr);
    EXPECT_EQ(rjk::get_if<Beta>(&x), nullptr);
}

TEST(DuckQualifiers, ConstMethod) {
    struct WithConst {
        int value = 42;
        auto doSmth() const -> int { return value; }
    };

    rjk::duck<rjk::policy<rjk::has_fn<"doSmth", int() const>>> d{WithConst{}};
    EXPECT_EQ(d.doSmth(), 42);
}

TEST(DuckQualifiers, ConstMethodOnConstDuck) {
    struct WithConst {
        int value = 42;
        auto doSmth() const -> int { return value; }
    };

    const rjk::duck<rjk::policy<rjk::has_fn<"doSmth", int() const>>> d{WithConst{}};
    EXPECT_EQ(d.doSmth(), 42);
}

TEST(DuckQualifiers, LvalueRefQualifiedMethod) {
    struct WithLvalueRef {
        auto lvalue_fn() & -> int { return 1; }
    };

    rjk::duck<rjk::policy<rjk::has_fn<"lvalue_fn", int() &>>> d{WithLvalueRef{}};
    EXPECT_EQ(d.lvalue_fn(), 1);
}

TEST(DuckQualifiers, RvalueRefQualifiedMethod) {
    struct WithRvalueRef {
        auto rvalue_fn() && -> int { return 2; }
    };

    rjk::duck<rjk::policy<rjk::has_fn<"rvalue_fn", int() &&>>> d{WithRvalueRef{}};
    EXPECT_EQ(std::move(d).rvalue_fn(), 2);
}

TEST(DuckQualifiers, LvalueReturningMethodIsAssignableThrough) {
    struct WithMutableRef {
        int x = 10;
        auto lvalue_ret() -> int& { return x; }
    };

    rjk::duck<rjk::policy<rjk::has_fn<"lvalue_ret", int&()>>> d{WithMutableRef{}};
    EXPECT_EQ(d.lvalue_ret(), 10);
    d.lvalue_ret() = 5;
    EXPECT_EQ(d.lvalue_ret(), 5);
}

TEST(DuckQualifiers, NoexceptMethodPropagatesNoexcept) {
    struct [[=rjk::trait]] NoexceptTrait {
        auto foo() noexcept -> int;
    };

    struct WithNoexcept {
        auto foo() noexcept -> int { return 10; }
    };

    rjk::duck<NoexceptTrait> d{WithNoexcept{}};
    EXPECT_EQ(d.foo(), 10);
    static_assert(noexcept(d.foo()));
}

}