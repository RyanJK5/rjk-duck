#include "factories.hpp"

namespace rjk_bench {

struct ConcreteCounter {
    int data{};

    auto getData() const -> int {
        return data;
    }
};

auto MakeDuckCounter(int initial) -> rjk::duck<Counter> {
    return rjk::duck<Counter>{std::in_place_type<ConcreteCounter>, initial};
}

auto MakeInlineDuckCounter(int initial) -> rjk::duck<Counter, CounterPerf> {
    return rjk::duck<Counter, CounterPerf>{std::in_place_type<ConcreteCounter>, initial};
}

struct CallableCounter {
    int data{};

    auto operator()() const -> int {
        return data;
    }
};

auto MakeFuncCounter(int initial) -> std::function<int()> {
    return std::function<int()>{CallableCounter{initial}};
}

struct VirtualCounter final : ICounter {
    int data{};

    explicit VirtualCounter(int d) : data(d) { }

    auto getData() const -> int {
        return data;
    }
};

auto MakeVirtualCounter(int initial) -> std::unique_ptr<ICounter> {
    return std::make_unique<VirtualCounter>(initial);
}

}