#include <benchmark/benchmark.h>
#include <rjk/duck.hpp>

#include "factories.hpp"

namespace rjk_bench {

static int seed = 1298433875; // Arbitrary runtime value

static void BM_DuckCall(benchmark::State& state) {
    auto seed = state.range(0);
    auto d = MakeDuckCounter(seed);

    for (auto _ : state) {
        auto result = d.getData();
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_DuckCall)->Arg(seed);

static void BM_InlineDuckCall(benchmark::State& state) {
    auto seed = state.range(0);
    auto d = MakeInlineDuckCounter(seed);

    for (auto _ : state) {
        auto result = d.getData();
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_InlineDuckCall)->Arg(seed);

static void BM_VirtualCall(benchmark::State& state) {
    auto seed = state.range(0);
    auto v = MakeVirtualCounter(seed);

    for (auto _ : state) {
        auto result = v->getData();
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_VirtualCall)->Arg(seed);

static void BM_FunctionCall(benchmark::State& state) {
    auto seed = state.range(0);
    auto func = MakeFuncCounter(seed);

    for (auto _ : state) {
        auto result = func();
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_FunctionCall)->Arg(seed);

static void BM_AnyAnyCall(benchmark::State& state) {
    auto seed = state.range(0);
    auto aa = MakeAnyAny(seed);

    for (auto _ : state) {
        auto result = aa.getData();
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_AnyAnyCall)->Arg(seed);

static void BM_ProxyCall(benchmark::State& state) {
    auto seed = state.range(0);
    auto p = MakeProxyCounter(seed);

    for (auto _ : state) {
        auto result = p->getData();
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_ProxyCall)->Arg(seed);

BENCHMARK_MAIN();

}
