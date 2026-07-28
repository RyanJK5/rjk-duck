// test_allocator.cpp
#include "rjk/duck.hpp"

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

struct Stringify {
    auto to_string() const -> std::string;
};

// Fits the default SBO buffer.
struct Widget {
    int value{};
    auto to_string() const -> std::string { return "Widget(" + std::to_string(value) + ")"; }
};

// Deliberately larger than the default sbo_size, so it always forces the
// allocator path. Carries a tag so tests can tell distinct instances apart
// after copies/moves/reassignments.
struct BigWidget {
    std::array<char, 64> padding{};
    int tag{};

    explicit BigWidget(int t = 0) : tag(t) {}
    auto to_string() const -> std::string { return "BigWidget(" + std::to_string(tag) + ")"; }
};

struct AllocCounter {
    int allocs{};
    int deallocs{};
    int copies{};
    int moves{};

    int outstanding() const { return allocs - deallocs; }
};

template <typename T>
struct TestAlloc {
    using value_type = T;
    using is_always_equal = std::true_type;

    explicit TestAlloc(AllocCounter& counter) noexcept
        : m_counter(&counter)
    { }

    template <typename U>
    TestAlloc(const TestAlloc<U>& other) noexcept
        : m_counter(other.m_counter) {
        m_counter->copies++;
    }

    template <typename U>
    TestAlloc(TestAlloc<U>&& other) noexcept
        : m_counter(std::exchange(other.m_counter, nullptr)) {
        m_counter->moves++;
    }

    template <typename U>
    TestAlloc& operator=(const TestAlloc<U>& other) noexcept {
        if (static_cast<const void*>(this) != static_cast<const void*>(&other)) {
            m_counter = other.m_counter;
            m_counter->copies++;
        }
        return *this;
    }

    template <typename U>
    TestAlloc& operator=(TestAlloc<U>&& other) noexcept {
        if (static_cast<const void*>(this) != static_cast<const void*>(&other)) {
            m_counter = std::exchange(other.m_counter, nullptr);
            m_counter->moves++;
        }
        return *this;
    }

    T* allocate(std::size_t n) {
        if (m_counter) {
            m_counter->allocs++;
        }
        return static_cast<T*>(::operator new(n * sizeof(T)));
    }

    void deallocate(T* p, std::size_t) noexcept {
        if (m_counter) {
            m_counter->deallocs++;
        }
        ::operator delete(p);
    }

    template <typename U>
    bool operator==(const TestAlloc<U>& other) const noexcept {
        return m_counter == other.m_counter;
    }

private:
    template <typename U>
    friend struct TestAlloc;

    AllocCounter* m_counter;
};

struct [[=rjk::perf_options]] TestAllocTrait {
    using allocator = TestAlloc<std::byte>;
};

using TestDuck = rjk::duck<Stringify, TestAllocTrait>;
using CopyableTestDuck = rjk::duck<Stringify, rjk::copyable, TestAllocTrait>;

TEST(DuckAllocator, ConstructsWithAllocatorArgTag) {
    AllocCounter counter{};
    TestAlloc<std::byte> alloc{counter};

    TestDuck d{std::allocator_arg, alloc, BigWidget{1}};

    EXPECT_EQ(d.to_string(), "BigWidget(1)");
    EXPECT_EQ(counter.allocs, 1);
    EXPECT_EQ(counter.deallocs, 0);
}

TEST(DuckAllocator, DestructionDeallocatesThroughStoredAllocator) {
    AllocCounter counter{};
    TestAlloc<std::byte> alloc{counter};

    {
        TestDuck d{std::allocator_arg, alloc, BigWidget{2}};
        EXPECT_EQ(counter.allocs, 1);
        EXPECT_EQ(counter.deallocs, 0);
    }

    EXPECT_EQ(counter.deallocs, 1);
    EXPECT_EQ(counter.outstanding(), 0);
}

TEST(DuckAllocator, ReassigningNonSboTypeDeallocatesOldAndAllocatesNew) {
    AllocCounter counter{};
    TestAlloc<std::byte> alloc{counter};

    TestDuck d{std::allocator_arg, alloc, BigWidget{3}};
    ASSERT_EQ(counter.allocs, 1);
    ASSERT_EQ(counter.deallocs, 0);

    d = BigWidget{4};

    EXPECT_EQ(d.to_string(), "BigWidget(4)");
    EXPECT_EQ(counter.allocs, 2);
    EXPECT_EQ(counter.deallocs, 1);
}

TEST(DuckAllocator, ReassigningToSboTypeSkipsAllocationButStillFreesOld) {
    AllocCounter counter{};
    TestAlloc<std::byte> alloc{counter};

    TestDuck d{std::allocator_arg, alloc, BigWidget{5}};
    ASSERT_EQ(counter.allocs, 1);

    d = Widget{99};

    EXPECT_EQ(d.to_string(), "Widget(99)");
    EXPECT_EQ(counter.allocs, 1);
    EXPECT_EQ(counter.deallocs, 1);
}

TEST(DuckAllocator, EmplaceRoutesThroughAllocator) {
    AllocCounter counter{};
    TestAlloc<std::byte> alloc{counter};

    TestDuck d{std::allocator_arg, alloc, Widget{1}};
    ASSERT_EQ(counter.allocs, 0);

    rjk::emplace<BigWidget>(d, 7);

    EXPECT_EQ(d.to_string(), "BigWidget(7)");
    EXPECT_EQ(counter.allocs, 1);
    EXPECT_EQ(counter.deallocs, 0);
}

// ---------------------------------------------------------------------
// Copy / move
// ---------------------------------------------------------------------

TEST(DuckAllocator, CopyConstructionCopiesAllocatorAndAllocatesForTheCopy) {
    AllocCounter counter{};
    TestAlloc<std::byte> alloc{counter};

    CopyableTestDuck original{std::allocator_arg, alloc, BigWidget{11}};
    ASSERT_EQ(counter.allocs, 1);

    CopyableTestDuck copy{original};

    EXPECT_EQ(copy.to_string(), "BigWidget(11)");
    EXPECT_GE(counter.copies, 1);
    EXPECT_EQ(counter.allocs, 2);   // fresh storage for the copy's BigWidget
    EXPECT_EQ(counter.deallocs, 0); // original is untouched
}

TEST(DuckAllocator, MoveConstructionTransfersAllocatorWithoutReallocating) {
    AllocCounter counter{};
    TestAlloc<std::byte> alloc{counter};

    TestDuck original{std::allocator_arg, alloc, BigWidget{22}};
    ASSERT_EQ(counter.allocs, 1);

    TestDuck moved{std::move(original)};

    EXPECT_EQ(moved.to_string(), "BigWidget(22)");
    // Moving transplants the same heap block; no new alloc/dealloc.
    EXPECT_EQ(counter.allocs, 1);
    EXPECT_EQ(counter.deallocs, 0);
}

class CountingResource : public std::pmr::memory_resource {
public:
    explicit CountingResource(std::pmr::memory_resource* upstream)
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

    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override {
        return this == &other;
    }

    std::pmr::memory_resource* m_upstream;
};

struct [[=rjk::perf_options]] PmrPerf {
    using allocator = std::pmr::polymorphic_allocator<std::byte>;
};

TEST(DuckAllocator, UsesPmrAllocatorForNonSboTypes) {
    std::array<std::byte, 512> buffer{};
    std::pmr::monotonic_buffer_resource upstream{buffer.data(), buffer.size()};
    CountingResource counting{&upstream};
    std::pmr::polymorphic_allocator alloc{&counting};

    rjk::duck<Stringify, PmrPerf> d{std::allocator_arg, alloc, BigWidget{123}};

    EXPECT_EQ(d.to_string(), "BigWidget(123)");
    EXPECT_EQ(counting.allocs, 1);
    EXPECT_EQ(counting.deallocs, 0);
}

TEST(DuckAllocator, PmrAllocatorDeallocatesOnDestruction) {
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

}  // namespace rjk_test