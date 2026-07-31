#include "fixtures.hpp"

#include <gtest/gtest.h>

namespace rjk_test {

struct AllocatorThrowException : std::bad_alloc {
    const char* what() const noexcept override { return "ThrowingTestAlloc: forced allocation failure"; }
};

template <typename T>
struct ThrowingTestAlloc {
    using value_type = T;
    using propagate_on_container_copy_assignment = std::false_type;
    using propagate_on_container_move_assignment = std::false_type;
    using propagate_on_container_swap = std::false_type;

    template <typename U>
    struct rebind {
        using other = ThrowingTestAlloc<U>;
    };

    ThrowingTestAlloc(AllocCounter& counter, const bool& should_throw) noexcept
        : m_counter(&counter), m_should_throw(&should_throw)
    { }

    template <typename U>
    ThrowingTestAlloc(const ThrowingTestAlloc<U>& other) noexcept
        : m_counter(other.m_counter), m_should_throw(other.m_should_throw)
    { }

    T* allocate(std::size_t n) {
        if (m_should_throw && *m_should_throw) {
            throw AllocatorThrowException{};
        }
        m_counter->allocs++;
        return static_cast<T*>(::operator new(n * sizeof(T)));
    }

    void deallocate(T* p, std::size_t) noexcept {
        m_counter->deallocs++;
        ::operator delete(p);
    }

    template <typename U>
    bool operator==(const ThrowingTestAlloc<U>& other) const noexcept {
        return m_counter == other.m_counter;
    }

    AllocCounter* m_counter;
    const bool* m_should_throw;
};

using ThrowingAlloc = ThrowingTestAlloc<std::byte>;
struct [[=rjk::perf_options]] Throwing {
    using allocator = ThrowingAlloc;
};

TEST(AllocatorExceptionSafety, FailedAllocationDuringConstructionThrowsAndLeaksNothing) {
    AllocCounter counter{};
    bool shouldThrow = true;
    ThrowingAlloc alloc{counter, shouldThrow};

    EXPECT_THROW(
        (AllocDuck<Throwing>{std::allocator_arg, alloc, BigWidget{901}}),
        AllocatorThrowException);

    EXPECT_EQ(counter.allocs, 0);
    EXPECT_EQ(counter.deallocs, 0);
}

// If the destination's allocator fails to obtain storage for the incoming
// value, the source must be left completely intact (strong exception
// guarantee: no partial mutation of `a`).
TEST(AllocatorExceptionSafety, FailedAllocationDuringCopyAssignmentLeavesSourceIntact) {
    AllocCounter counterA{};
    AllocCounter counterB{};
    bool shouldThrowA = false;
    bool shouldThrowB = false;
    ThrowingAlloc allocA{counterA, shouldThrowA};
    ThrowingAlloc allocB{counterB, shouldThrowB};

    AllocDuck<Throwing> a{std::allocator_arg, allocA, BigWidget{902}};
    AllocDuck<Throwing> b{std::allocator_arg, allocB, BigWidget{903}};
    ASSERT_EQ(counterA.allocs, 1);
    ASSERT_EQ(counterB.allocs, 1);

    shouldThrowB = true; // b's allocator will fail to obtain storage for the copy

    EXPECT_THROW(b = a, AllocatorThrowException);

    // The source must be completely unaffected by a failed assignment into b.
    EXPECT_EQ(a.to_string(), "BigWidget(902)");
    EXPECT_EQ(counterA.allocs, 1);
    EXPECT_EQ(counterA.deallocs, 0);

    // Weak exception guarantee: b is not in the same state
    EXPECT_EQ(counterB.deallocs, 1);
    EXPECT_TRUE(rjk::valueless_after_move(b));
}

// A failed emplace() must not leave the duck holding a torn or
// double-freed value; at minimum, allocs and deallocs must stay balanced
// for whatever storage genuinely changed hands.
TEST(AllocatorExceptionSafety, FailedAllocationDuringEmplaceThrowsAndLeaksNothing) {
    AllocCounter counter{};
    bool shouldThrow = false;
    ThrowingAlloc alloc{counter, shouldThrow};

    AllocDuck<Throwing> d{std::allocator_arg, alloc, Widget{904}}; // SBO, no alloc yet
    ASSERT_EQ(counter.allocs, 0);

    shouldThrow = true;

    EXPECT_THROW(rjk::emplace<BigWidget>(d, 905), AllocatorThrowException);

    EXPECT_EQ(counter.allocs, 0);
    EXPECT_EQ(counter.deallocs, 0);
}

}  // namespace rjk_test