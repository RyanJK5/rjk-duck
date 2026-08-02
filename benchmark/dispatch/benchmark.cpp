#include <benchmark/benchmark.h>
#include <rjk/duck.hpp>

#include "factories.hpp"

namespace rjk_bench {

static int seed = 30; // Arbitrary runtime value

static void BM_DuckCall(benchmark::State& state) {
    auto seed = state.range(0);
    auto d = MakeDuckCounter(seed);

    for (auto _ : state) {
        benchmark::DoNotOptimize(d.getData());
    }
}
BENCHMARK(BM_DuckCall)->Arg(seed);

static void BM_InlineDuckCall(benchmark::State& state) {
    auto seed = state.range(0);
    auto d = MakeInlineDuckCounter(seed);

    for (auto _ : state) {
        benchmark::DoNotOptimize(d.getData());
    }
}
BENCHMARK(BM_InlineDuckCall)->Arg(seed);

static void BM_VirtualCall(benchmark::State& state) {
    auto seed = state.range(0);
    auto v = MakeVirtualCounter(seed);

    for (auto _ : state) {
        benchmark::DoNotOptimize(v->getData());
    }
}
BENCHMARK(BM_VirtualCall)->Arg(seed);

static void BM_FunctionCall(benchmark::State& state) {
    auto seed = state.range(0);
    auto func = MakeFuncCounter(seed);

    for (auto _ : state) {
        benchmark::DoNotOptimize(func());
    }
}
BENCHMARK(BM_FunctionCall)->Arg(seed);

BENCHMARK_MAIN();

}
