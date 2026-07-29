// test_allocator.cpp
#include "rjk/duck.hpp"
#include "fixtures.hpp"

#include <array>
#include <concepts>
#include <cstddef>
#include <gtest/gtest.h>
#include <memory>
#include <memory_resource>
#include <string>
#include <type_traits>
#include <utility>

namespace rjk_test {

class CountingResource : public std::pmr::memory_resource {
public:
    explicit CountingResource(memory_resource* upstream)
        : m_upstream(upstream)
    { }

    int allocs{};
    int deallocs{};

private:
    void* do_allocate(std::size_t bytes, std::size_t alignment) override {
        allocs++;
        return m_upstream->allocate(bytes, alignment);
    }

    void do_deallocate(void* p, std::size_t bytes, std::size_t alignment) override {
        deallocs++;
        m_upstream->deallocate(p, bytes, alignment);
    }

    bool do_is_equal(const memory_resource& other) const noexcept override {
        return this == &other;
    }

    memory_resource* m_upstream;
};

struct [[=rjk::perf_options]] PmrPerf {
    using allocator = std::pmr::polymorphic_allocator<std::byte>;
};

TEST(PmrAllocator, Constructor) {
    std::array<std::byte, 512> buffer{};
    std::pmr::monotonic_buffer_resource upstream{buffer.data(), buffer.size()};
    CountingResource counting{&upstream};
    std::pmr::polymorphic_allocator alloc{&counting};

    rjk::duck<Stringify, PmrPerf> d{std::allocator_arg, alloc, BigWidget{123}};

    EXPECT_EQ(d.to_string(), "BigWidget(123)");
    EXPECT_EQ(counting.allocs, 1);
    EXPECT_EQ(counting.deallocs, 0);
}

TEST(PmrAllocator, Destructor) {
    std::array<std::byte, 512> buffer{};
    std::pmr::monotonic_buffer_resource upstream{buffer.data(), buffer.size()};
    CountingResource counting{&upstream};
    std::pmr::polymorphic_allocator alloc{&counting};

    {
        rjk::duck<Stringify, PmrPerf> d{std::allocator_arg, alloc, BigWidget{7}};
        EXPECT_EQ(counting.allocs, 1);
        EXPECT_EQ(counting.deallocs, 0);
    }

    EXPECT_EQ(counting.deallocs, 1);
}

TEST(PmrAllocator, PmrMoveAssignmentReallocatesAcrossDifferentResources) {
    std::array<std::byte, 512> bufferA{};
    std::array<std::byte, 512> bufferB{};
    std::pmr::monotonic_buffer_resource upstreamA{bufferA.data(), bufferA.size()};
    std::pmr::monotonic_buffer_resource upstreamB{bufferB.data(), bufferB.size()};
    CountingResource resourceA{&upstreamA};
    CountingResource resourceB{&upstreamB};
    std::pmr::polymorphic_allocator allocA{&resourceA};
    std::pmr::polymorphic_allocator allocB{&resourceB};

    rjk::duck<Stringify, PmrPerf> a{std::allocator_arg, allocA, BigWidget{141}};
    rjk::duck<Stringify, PmrPerf> b{std::allocator_arg, allocB, BigWidget{142}};
    ASSERT_EQ(resourceA.allocs, 1);
    ASSERT_EQ(resourceB.allocs, 1);

    b = std::move(a);

    EXPECT_EQ(b.to_string(), "BigWidget(141)");
    EXPECT_EQ(resourceB.allocs, 2);
    EXPECT_EQ(resourceB.deallocs, 1);
    EXPECT_EQ(resourceA.deallocs, 1);
}

}  // namespace rjk_test