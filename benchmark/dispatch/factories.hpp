// Using factory functions prevents the compiler from knowing what type is
// underlying a type-erased container.

#ifndef RJK_BENCH_FACTORIES_HPP
#define RJK_BENCH_FACTORIES_HPP

#include "rjk/duck.hpp"
#include <anyany/anyany.hpp>
#include <anyany/anyany_macro.hpp>
#include <functional>
#include <memory>
#include <proxy/proxy.h>

namespace rjk_bench {

// Duck

template <bool Direct = false>
struct Counter;

template <>
struct Counter<false> {
    auto getData() const -> int;
};

template <>
struct Counter<true> {
    [[=rjk::direct]]
    auto getData() const -> int;
};

using DuckCounter = rjk::duck<Counter<>>;
auto MakeDuckCounter(int initial) -> DuckCounter;

using DirectDuckCounter = rjk::duck<Counter<true>>;
auto MakeInlineDuckCounter(int initial) -> DirectDuckCounter;

// std::function
auto MakeFuncCounter(int initial) -> std::function<int()>;

// Virtual function
struct ICounter {
    virtual ~ICounter() = default;
    virtual auto getData() const -> int = 0;
};

auto MakeVirtualCounter(int initial) -> std::unique_ptr<ICounter>;

// AnyAny
anyany_method(getData, (&self) requires(self.getData()) -> int);
using AnyAnyCounter = aa::any_with<getData>;

auto MakeAnyAny(int initial) -> AnyAnyCounter;

struct AACounter {
    template <typename T>
    static void do_invoke(const T& self) {
        return self.getData();
    }
};

// Proxy
PRO_DEF_MEM_DISPATCH(MemGetData, getData);

struct CounterFacade : pro::facade_builder
    ::add_convention<MemGetData, int() const>
    ::build {};

using ProxyCounter = pro::proxy<CounterFacade>;
auto MakeProxyCounter(int initial) -> ProxyCounter;

static_assert(sizeof(DuckCounter) == 32);
static_assert(sizeof(DirectDuckCounter) == 40);

static_assert(sizeof(AnyAnyCounter) == 48);
static_assert(sizeof(ProxyCounter) == 32);
}

#endif
