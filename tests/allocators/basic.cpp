#include "rjk/duck.hpp"
#include "fixtures.hpp"

#include <gtest/gtest.h>

namespace rjk_test {

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
        if (this != &other) {
            m_counter = other.m_counter;
            m_counter->copies++;
        }
        return *this;
    }

    template <typename U>
    TestAlloc& operator=(TestAlloc<U>&& other) noexcept {
        if (this != &other) {
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
        return true;
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

TEST(BasicAllocator, AllocatorConstructor) {
    AllocCounter counter{};
    TestAlloc<std::byte> alloc{counter};

    TestDuck d{std::allocator_arg, alloc, BigWidget{1}};

    EXPECT_EQ(d.to_string(), "BigWidget(1)");
    EXPECT_EQ(counter.allocs, 1);
    EXPECT_EQ(counter.deallocs, 0);
}

TEST(BasicAllocator, AllocatorDestructor) {
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

TEST(BasicAllocator, ReassigningHeapAlloc) {
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

TEST(BasicAllocator, ReassigningToSbo) {
    AllocCounter counter{};
    TestAlloc<std::byte> alloc{counter};

    TestDuck d{std::allocator_arg, alloc, BigWidget{5}};
    ASSERT_EQ(counter.allocs, 1);

    d = Widget{99};

    EXPECT_EQ(d.to_string(), "Widget(99)");
    EXPECT_EQ(counter.allocs, 1);
    EXPECT_EQ(counter.deallocs, 1);
}

TEST(BasicAllocator, Emplace) {
    AllocCounter counter{};
    TestAlloc<std::byte> alloc{counter};

    TestDuck d{std::allocator_arg, alloc, Widget{1}};
    ASSERT_EQ(counter.allocs, 0);

    rjk::emplace<BigWidget>(d, 7);

    EXPECT_EQ(d.to_string(), "BigWidget(7)");
    EXPECT_EQ(counter.allocs, 1);
    EXPECT_EQ(counter.deallocs, 0);
}

TEST(BasicAllocator, CopyConstruct) {
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

TEST(BasicAllocator, MoveConstruct) {
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

}