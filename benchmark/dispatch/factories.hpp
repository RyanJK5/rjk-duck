// Using factory functions prevents the compiler from knowing what type is
// underlying a type-erased container.

#ifndef RJK_BENCH_FACTORIES_HPP
#define RJK_BENCH_FACTORIES_HPP

#include "rjk/duck.hpp"
#include <functional>
#include <memory>

namespace rjk_bench {

template <bool Direct = false>
struct Counter {
    [[=rjk::direct(Direct)]]
    auto getData() const -> int;
};

using DuckCounter = rjk::duck<Counter<>>;
auto MakeDuckCounter(int initial) -> DuckCounter;

using DirectDuckCounter = rjk::duck<Counter<true>>;
auto MakeInlineDuckCounter(int initial) -> DirectDuckCounter;

auto MakeFuncCounter(int initial) -> std::function<int()>;

struct ICounter {
    virtual ~ICounter() = default;
    virtual auto getData() const -> int = 0;
};

auto MakeVirtualCounter(int initial) -> std::unique_ptr<ICounter>;

}

#endif
