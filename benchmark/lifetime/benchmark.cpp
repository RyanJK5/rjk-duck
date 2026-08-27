#include <benchmark/benchmark.h>

#include <anyany/anyany.hpp>
#include <anyany/anyany_macro.hpp>
#include <functional>
#include <memory>
#include <proxy/proxy.h>
#include <cstddef>
#include <new>
#include <rjk/duck.hpp>

namespace rjk_bench {

template <std::size_t Size>
struct alignas(std::size_t) Payload {
    std::byte data[Size];
    auto getData() const -> int { return std::to_integer<int>(data[0]); }
    auto operator()() const -> int { return std::to_integer<int>(data[0]); }
};

// Duck
template <bool Direct = false>
struct Counter {
    [[=rjk::direct(Direct)]]
    auto getData() const -> int;
};

// Virtual function
struct ICounter {
    virtual ~ICounter() = default;
    virtual auto getData() const -> int = 0;
};

template <std::size_t Size>
struct VirtualPayload final : ICounter {
    std::byte data[Size]{};
    auto getData() const -> int override { return std::to_integer<int>(data[0]); }
};

// AnyAny
anyany_method(getData, (&self) requires(self.getData()) -> int);
using AnyAnyCounter = aa::any_with<getData>;

// Proxy
PRO_DEF_MEM_DISPATCH(MemGetData, getData);
struct CounterFacade : pro::facade_builder
    ::add_convention<MemGetData, int() const>
    ::build {};
using ProxyCounter = pro::proxy<CounterFacade>;

template <typename T, std::size_t N>
constexpr static auto ConstructPayload(T* ptr) {
    if constexpr(std::same_as<T, rjk::duck<Counter<>>>
            || std::same_as<T, rjk::duck<Counter<true>>>) {
        return std::construct_at(ptr, std::in_place_type<Payload<N>>);
    } else if constexpr (std::same_as<T, std::function<int()>>) {
        return std::construct_at(ptr, Payload<N>{});
    } else if constexpr (std::same_as<T, AnyAnyCounter>) {
        return std::construct_at(ptr, std::in_place_type<Payload<N>>);
    } else if constexpr (std::same_as<T, ProxyCounter>) {
        return std::construct_at(ptr, pro::make_proxy<CounterFacade, Payload<N>>());
    } else {
        return std::construct_at(ptr, std::make_unique<VirtualPayload<N>>());
    }
}

constexpr int BatchSize = 512;

template <typename T>
T* slot(std::byte* base, int i) {
    return std::launder(reinterpret_cast<T*>(base) + i);
}

template <typename T, std::size_t Size>
static void BM_Construct(benchmark::State& state) {
    alignas(T) std::array<std::byte, sizeof(T) * BatchSize> storage;
    int i{};

    for (auto _ : state) {
        auto* address = storage.data() + (i * sizeof(T));
        T* ptr = ConstructPayload<T, Size>(reinterpret_cast<T*>(address));
        benchmark::DoNotOptimize(*ptr);

        if (++i == BatchSize) {
            state.PauseTiming();
            for (int j = 0; j < BatchSize; ++j) {
                std::destroy_at(slot<T>(storage.data(), j));
            }
            i = 0;
            state.ResumeTiming();
        }
    }
}

template <typename T, std::size_t Size>
static void BM_Destruct(benchmark::State& state) {
    alignas(T) std::array<std::byte, sizeof(T) * BatchSize> storage;
    int i{};

    for (auto _ : state) {
        if (i == 0) {
            state.PauseTiming();
            for (int j = 0; j < BatchSize; ++j) {
                auto* address = storage.data() + (j * sizeof(T));
                ConstructPayload<T, Size>(reinterpret_cast<T*>(address));

            }
            state.ResumeTiming();
        }
        T* ptr = slot<T>(storage.data(), i);
        benchmark::DoNotOptimize(*ptr);
        std::destroy_at(ptr);
        i = (i + 1) % BatchSize;
    }
}

#define BENCH_ALL(N)                                                           \
    BENCHMARK_TEMPLATE(BM_Construct, rjk::duck<Counter<>>, N);                 \
    BENCHMARK_TEMPLATE(BM_Construct, rjk::duck<Counter<true>>, N);             \
    BENCHMARK_TEMPLATE(BM_Construct, std::unique_ptr<ICounter>, N);            \
    BENCHMARK_TEMPLATE(BM_Construct, std::function<int()>, N);                 \
    BENCHMARK_TEMPLATE(BM_Construct, AnyAnyCounter, N);                        \
    BENCHMARK_TEMPLATE(BM_Construct, ProxyCounter, N);                         \
                                                                               \
    BENCHMARK_TEMPLATE(BM_Destruct, rjk::duck<Counter<>>, N);                  \
    BENCHMARK_TEMPLATE(BM_Destruct, rjk::duck<Counter<true>>, N);              \
    BENCHMARK_TEMPLATE(BM_Destruct, std::unique_ptr<ICounter>, N);             \
    BENCHMARK_TEMPLATE(BM_Destruct, std::function<int()>, N);                  \
    BENCHMARK_TEMPLATE(BM_Destruct, AnyAnyCounter, N);                         \
    BENCHMARK_TEMPLATE(BM_Destruct, ProxyCounter, N);                          \

BENCH_ALL(8);
BENCH_ALL(16);
BENCH_ALL(32);
BENCH_ALL(64);
BENCH_ALL(128);

BENCHMARK_MAIN();

} // namespace rjk_bench