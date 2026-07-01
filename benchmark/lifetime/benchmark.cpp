#include <benchmark/benchmark.h>
#include <functional>
#include <memory>
#include <array>
#include <cstddef>
#include <new>
#include <rjk/duck.hpp>

namespace rjk_bench {

struct [[=rjk::trait]] Counter {
    auto getData() const -> int;
};

template <std::size_t Size>
struct alignas(std::size_t) Payload {
    std::byte data[Size]{};
    auto getData() const -> int { return std::to_integer<int>(data[0]); }
    auto operator()() const -> int { return std::to_integer<int>(data[0]); }
};

struct ICounter {
    virtual ~ICounter() = default;
    virtual auto getData() const -> int = 0;
};

template <std::size_t Size>
struct VirtualPayload final : ICounter {
    std::byte data[Size]{};
    auto getData() const -> int override { return std::to_integer<int>(data[0]); }
};

template <typename T, std::size_t N>
constexpr static auto ConstructPayload() {
    if constexpr(std::same_as<T, rjk::duck<Counter>>) {
        return std::in_place_type<Payload<N>>;
    } else if constexpr (std::same_as<T, std::function<int()>>) {
        return Payload<N>{};
    } else {
        return std::make_unique<VirtualPayload<N>>();
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
        T* ptr = std::construct_at(reinterpret_cast<T*>(storage.data()) + i,
            std::invoke(ConstructPayload<T, Size>));
        benchmark::DoNotOptimize(*ptr);

        if (++i == BatchSize) {
            state.PauseTiming();
            for (int j = 0; j < BatchSize; ++j)
                std::destroy_at(slot<T>(storage.data(), j));
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
            for (int j = 0; j < BatchSize; ++j)
                std::construct_at(reinterpret_cast<T*>(storage.data()) + j,
                    std::invoke(ConstructPayload<T, Size>));
            state.ResumeTiming();
        }
        T* ptr = slot<T>(storage.data(), i);
        benchmark::DoNotOptimize(*ptr);
        std::destroy_at(ptr);
        i = (i + 1) % BatchSize;
    }
}

#define BENCH_ALL(N)                                                           \
    BENCHMARK_TEMPLATE(BM_Construct, std::unique_ptr<ICounter>, N);            \
    BENCHMARK_TEMPLATE(BM_Construct, rjk::duck<Counter>, N);                   \
    BENCHMARK_TEMPLATE(BM_Construct, std::function<int()>, N);                 \
                                                                               \
    BENCHMARK_TEMPLATE(BM_Destruct, std::unique_ptr<ICounter>, N);             \
    BENCHMARK_TEMPLATE(BM_Destruct, rjk::duck<Counter>, N);                    \
    BENCHMARK_TEMPLATE(BM_Destruct, std::function<int()>, N)                   \

BENCH_ALL(8);
BENCH_ALL(16);
BENCH_ALL(32);
BENCH_ALL(57); // Test one oddly-shaped object
BENCH_ALL(64);
BENCH_ALL(128);

BENCHMARK_MAIN();

} // namespace rjk_bench