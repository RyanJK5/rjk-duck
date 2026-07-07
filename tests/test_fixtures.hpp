#ifndef RJK_TEST_FIXTURES_HPP
#define RJK_TEST_FIXTURES_HPP

#include <concepts>
#include <memory>

// Subjectively, I've chosen not to nest tests in the rjk namespace so we can
// visually see what real code would look like.
namespace rjk_test {

struct A {
    inline static thread_local int instance_count = 0;

    A() { ++instance_count; }
    ~A() { --instance_count; }
    A(const A&) { ++instance_count; }
    A(A&&) noexcept { ++instance_count; }

    A& operator=(const A&) { return *this; }
    A& operator=(A&&) noexcept { return *this; }

    void test() {}

    int other(char) { return 10; }

    void consume() {}
};

struct B {
    void test() {}

    int other(char) { return 3; }

    void consume() {}
};

// Larger than SBO (32 bytes)
struct Big {
    std::array<std::byte, 64> data{};

    void test() {}

    int other(char) { return 99; }

    void consume() {}
};

// Move-only
struct MoveOnly {
    std::unique_ptr<int> val;

    explicit MoveOnly(int x) : val(std::make_unique<int>(x)) {}

    MoveOnly(const MoveOnly&) = delete;

    MoveOnly& operator=(const MoveOnly&) = delete;

    MoveOnly(MoveOnly&&) = default;

    MoveOnly& operator=(MoveOnly&&) = default;

    void test() {}

    int other(char) { return *val; }

    void consume() {}
};

// const-qualified method
struct WithConst {
    int value = 42;
    int doSmth() const { return value; }
};

// ref-qualified methods
struct WithLvalueRef {
    int lvalue_fn() & { return 1; }
};

struct WithRvalueRef {
    int rvalue_fn() && { return 2; }
};

struct [[=rjk::trait]] TestPolicy {
    auto test() -> void;
    auto other(char) -> int;
};

// Distinct alias for tests mixing SBO and heap types
struct [[=rjk::trait]] BigPolicy {
    auto test() -> void;
    auto other(char) -> int;
};
    
using TestDuck = rjk::duck<TestPolicy, rjk::copyable>;
using BigDuck = rjk::duck<BigPolicy>;

static_assert(sizeof(BigDuck) == sizeof(TestDuck));
static_assert(sizeof(TestDuck) == sizeof(rjk::duck<rjk::policy<
    rjk::has_fn<"duck", void() const>,
    rjk::has_fn<"with", int(const char&)>,
    rjk::has_fn<"many", bool(char)>,
    rjk::has_fn<"parameters", TestDuck() const>,
    rjk::has_op<rjk::op_plus, void(rjk::self&, int)>
>>));
static_assert(sizeof(TestDuck) <= 48);
}

#endif
