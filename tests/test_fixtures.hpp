#ifndef RJK_TEST_FIXTURES_HPP
#define RJK_TEST_FIXTURES_HPP

#include <memory>

// Subjectively, I've chosen not to nest tests in the rjk namespace so we can
// visually see what real code would look like.
namespace rjk_test {
// ── Test fixtures ────────────────────────────────────────────────────────────

struct A {
    inline static thread_local int instance_count = 0;

    A() { ++instance_count; }
    ~A() { --instance_count; }
    A(const A&) { ++instance_count; }
    A(A&&) noexcept { ++instance_count; }

    A& operator=(const A&) = default;

    A& operator=(A&&) = default;

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
    int get() const { return value; }
};

// ref-qualified methods
struct WithLvalueRef {
    int lvalue_fn() & { return 1; }
};

struct WithRvalueRef {
    int rvalue_fn() && { return 2; }
};

using TestDuck = rjk::duck<
    rjk::has_fn<"test", void()>,
    rjk::has_fn<"other", int(char)>
>;

// Distinct alias for tests mixing SBO and heap types
using BigDuck = rjk::duck<
    rjk::has_fn<"test", void()>,
    rjk::has_fn<"other", int(char)>
>;
}

#endif //RJK_TEST_FIXTURES_HPP
