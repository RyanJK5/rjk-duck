#include "rjk/duck.hpp"

namespace rjk_test {

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

struct [[=rjk::trait]] StrictMatching {
    rjk::lookup_rule rule = rjk::lookup_rule::loose;

    int foo(int);
};

struct S {
    int foo(double);
};

static_assert(rjk::satisfies<S, StrictMatching>);

}