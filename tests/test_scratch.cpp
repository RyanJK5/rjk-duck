#include "rjk/duck.hpp"

struct [[=rjk::trait]] Trait {
    int foo();
};

struct A { constexpr int foo() { return 5; }};
// struct B { constexpr int foo() { return 10; }};
struct C : A { using A::foo; };

static_assert(rjk::duck_view<Trait>{C{}}.foo() == C{}.foo());