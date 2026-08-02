#ifndef RJK_BENCH_FIXTURES_HPP
#define RJK_BENCH_FIXTURES_HPP

namespace rjk_bench {

struct Counter {
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

}

#endif // RJK_BENCH_FIXTURES_HPP