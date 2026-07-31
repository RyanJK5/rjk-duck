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

TEST(PmrAllocator, PmrCopyAssignmentKeepsDestinationResource) {
    std::array<std::byte, 512> bufferA{};
    std::array<std::byte, 512> bufferB{};
    std::pmr::monotonic_buffer_resource upstreamA{bufferA.data(), bufferA.size()};
    std::pmr::monotonic_buffer_resource upstreamB{bufferB.data(), bufferB.size()};
    CountingResource resourceA{&upstreamA};
    CountingResource resourceB{&upstreamB};
    std::pmr::polymorphic_allocator allocA{&resourceA};
    std::pmr::polymorphic_allocator allocB{&resourceB};

    rjk::duck<Stringify, PmrPerf, rjk::copyable> a{std::allocator_arg, allocA, BigWidget{143}};
    rjk::duck<Stringify, PmrPerf, rjk::copyable> b{std::allocator_arg, allocB, BigWidget{144}};
    ASSERT_EQ(resourceA.allocs, 1);
    ASSERT_EQ(resourceB.allocs, 1);

    b = a;

    EXPECT_EQ(b.to_string(), "BigWidget(143)");
    EXPECT_EQ(resourceA.allocs, 1); // a is untouched
    EXPECT_EQ(resourceB.allocs, 2); // b reallocates via its own resource
    EXPECT_EQ(resourceB.deallocs, 1);
}

TEST(PmrAllocator, PlainCopyConstructionUsesDefaultResourceNotSourceResource) {
    std::array<std::byte, 512> bufferSrc{};
    std::array<std::byte, 512> bufferDefault{};
    std::pmr::monotonic_buffer_resource upstreamSrc{bufferSrc.data(), bufferSrc.size()};
    std::pmr::monotonic_buffer_resource upstreamDefault{bufferDefault.data(), bufferDefault.size()};
    CountingResource resourceSrc{&upstreamSrc};
    CountingResource resourceDefault{&upstreamDefault};

    auto* previousDefault = std::pmr::set_default_resource(&resourceDefault);
    struct DefaultResourceGuard {
        std::pmr::memory_resource* previous;
        ~DefaultResourceGuard() { std::pmr::set_default_resource(previous); }
    } guard{previousDefault};

    std::pmr::polymorphic_allocator allocSrc{&resourceSrc};
    rjk::duck<Stringify, PmrPerf, rjk::copyable> original{std::allocator_arg, allocSrc, BigWidget{151}};
    ASSERT_EQ(resourceSrc.allocs, 1);

    rjk::duck<Stringify, PmrPerf, rjk::copyable> copy{original}; // SOCCC

    EXPECT_EQ(copy.to_string(), "BigWidget(151)");
    EXPECT_EQ(resourceSrc.allocs, 1);     // source resource untouched by the copy
    EXPECT_EQ(resourceDefault.allocs, 1); // copy's storage came from the default resource
}

}  // namespace rjk_test