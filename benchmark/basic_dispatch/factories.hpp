// Using factory functions prevents the compiler from knowing what type is
// underlying a type-erased container.

#ifndef RJK_BENCH_FACTORIES_HPP
#define RJK_BENCH_FACTORIES_HPP

#include "rjk/duck.hpp"
#include <functional>
#include <memory>

namespace rjk_bench {

struct [[=rjk::trait]] Counter {
    auto getData() const -> int;
};

struct [[=rjk::perf_options]] CounterPerf {
    struct inlined_functions {
        auto getData() const -> int;
    };
};

auto MakeDuckCounter(int initial) -> rjk::duck<Counter>;

auto MakeInlineDuckCounter(int initial) -> rjk::duck<Counter, CounterPerf>;

auto MakeFuncCounter(int initial) -> std::function<int()>;

struct ICounter {
    virtual ~ICounter() = default;
    virtual auto getData() const -> int = 0;
};

auto MakeVirtualCounter(int initial) -> std::unique_ptr<ICounter>;

}

#endif
