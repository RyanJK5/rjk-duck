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

// ---------------------------------------------------------------------
// An allocator that is never "always equal" and has configurable
// propagate_on_container_copy_assignment / propagate_on_container_move_
// assignment. TestAlloc above hard-codes is_always_equal = true even
// though its operator== is identity-based, which makes it useless for
// exercising POCCA/POCMA or allocator-identity logic: a conforming
// container is entitled to skip all of that when is_always_equal holds.
// ---------------------------------------------------------------------

template <typename T, bool Pocca, bool Pocma>
struct PropagatingTestAlloc {
    using value_type = T;
    using propagate_on_container_copy_assignment = std::bool_constant<Pocca>;
    using propagate_on_container_move_assignment = std::bool_constant<Pocma>;
    using is_always_equal = std::false_type;

    template <typename U>
    struct rebind {
        using other = PropagatingTestAlloc<U, Pocca, Pocma>;
    };

    explicit PropagatingTestAlloc(AllocCounter& counter) noexcept
        : m_counter(&counter)
    { }

    template <typename U>
    PropagatingTestAlloc(const PropagatingTestAlloc<U, Pocca, Pocma>& other) noexcept
        : m_counter(other.m_counter) {
        m_counter->copies++;
    }

    template <typename U>
    PropagatingTestAlloc(PropagatingTestAlloc<U, Pocca, Pocma>&& other) noexcept
        : m_counter(std::exchange(other.m_counter, nullptr)) {
        m_counter->moves++;
    }

    template <typename U>
    PropagatingTestAlloc& operator=(const PropagatingTestAlloc<U, Pocca, Pocma>& other) noexcept {
        if (this != &other) {
            m_counter = other.m_counter;
            m_counter->copies++;
        }
        return *this;
    }

    template <typename U>
    PropagatingTestAlloc& operator=(PropagatingTestAlloc<U, Pocca, Pocma>&& other) noexcept {
        if (this != &other) {
            m_counter = std::exchange(other.m_counter, nullptr);
            m_counter->moves++;
        }
        return *this;
    }

    PropagatingTestAlloc(const PropagatingTestAlloc& other) noexcept
    : m_counter(other.m_counter) {
        m_counter->copies++;
    }

    PropagatingTestAlloc(PropagatingTestAlloc&& other) noexcept
        : m_counter(std::exchange(other.m_counter, nullptr)) {
        m_counter->moves++;
    }

    PropagatingTestAlloc& operator=(const PropagatingTestAlloc& other) noexcept {
        if (this != &other) {
            m_counter = other.m_counter;
            m_counter->copies++;
        }
        return *this;
    }

    PropagatingTestAlloc& operator=(PropagatingTestAlloc&& other) noexcept {
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
    bool operator==(const PropagatingTestAlloc<U, Pocca, Pocma>& other) const noexcept {
        return m_counter == other.m_counter;
    }

private:
    template <typename U, bool P, bool M>
    friend struct PropagatingTestAlloc;

    AllocCounter* m_counter;
};

struct [[=rjk::perf_options]] PoccaTruePocmaTrue {
    using allocator = PropagatingTestAlloc<std::byte, true, true>;
};
struct [[=rjk::perf_options]] PoccaTruePocmaFalse {
    using allocator = PropagatingTestAlloc<std::byte, true, false>;
};
struct [[=rjk::perf_options]] PoccaFalsePocmaTrue {
    using allocator = PropagatingTestAlloc<std::byte, false, true>;
};
struct [[=rjk::perf_options]] PoccaFalsePocmaFalse {
    using allocator = PropagatingTestAlloc<std::byte, false, false>;
};

using PoccaTruePocmaTrueDuck   = rjk::duck<Stringify, rjk::copyable, PoccaTruePocmaTrue>;
using PoccaTruePocmaFalseDuck  = rjk::duck<Stringify, rjk::copyable, PoccaTruePocmaFalse>;
using PoccaFalsePocmaTrueDuck  = rjk::duck<Stringify, rjk::copyable, PoccaFalsePocmaTrue>;
using PoccaFalsePocmaFalseDuck = rjk::duck<Stringify, rjk::copyable, PoccaFalsePocmaFalse>;

TEST(DuckAllocator, AllocatorExtendedCopyCtorUsesSuppliedAllocatorNotSources) {
    AllocCounter counterSrc{};
    AllocCounter counterDst{};
    PropagatingTestAlloc<std::byte, false, false> allocSrc{counterSrc};
    PropagatingTestAlloc<std::byte, false, false> allocDst{counterDst};

    PoccaFalsePocmaFalseDuck original{std::allocator_arg, allocSrc, BigWidget{31}};
    ASSERT_EQ(counterSrc.allocs, 1);

    // Allocator-extended copy: the copy's storage comes from allocDst
    // (the one explicitly supplied), not from original's allocator, and
    // original is left completely untouched.
    PoccaFalsePocmaFalseDuck copy{std::allocator_arg, allocDst, original};

    EXPECT_EQ(copy.to_string(), "BigWidget(31)");
    EXPECT_EQ(counterDst.allocs, 1);
    EXPECT_EQ(counterSrc.allocs, 1);
    EXPECT_EQ(counterSrc.deallocs, 0);
}

TEST(DuckAllocator, AllocatorExtendedMoveCtorWithUnequalAllocatorReallocates) {
    using IdentityDuck = rjk::duck<Stringify, PoccaFalsePocmaFalse>;

    AllocCounter counterSrc{};
    AllocCounter counterDst{};
    PropagatingTestAlloc<std::byte, false, false> allocSrc{counterSrc};
    PropagatingTestAlloc<std::byte, false, false> allocDst{counterDst};

    IdentityDuck original{std::allocator_arg, allocSrc, BigWidget{32}};
    ASSERT_EQ(counterSrc.allocs, 1);

    IdentityDuck moved{std::allocator_arg, allocDst, std::move(original)};

    EXPECT_EQ(moved.to_string(), "BigWidget(32)");
    EXPECT_EQ(counterDst.allocs, 1);
}

TEST(DuckAllocator, AllocatorExtendedMoveCtorWithEqualAllocatorStealsStorage) {
    using IdentityDuck = rjk::duck<Stringify, PoccaFalsePocmaFalse>;

    AllocCounter counter{};
    PropagatingTestAlloc<std::byte, false, false> allocA{counter};
    PropagatingTestAlloc<std::byte, false, false> allocB{counter}; //

    IdentityDuck original{std::allocator_arg, allocA, BigWidget{33}};
    ASSERT_EQ(counter.allocs, 1);

    // allocA == allocB (same underlying counter), so this should transplant
    // the existing heap block instead of allocating again.
    IdentityDuck moved{std::allocator_arg, allocB, std::move(original)};

    EXPECT_EQ(moved.to_string(), "BigWidget(33)");
    EXPECT_EQ(counter.allocs, 1);
    EXPECT_EQ(counter.deallocs, 0);
}

TEST(DuckAllocator, CopyAssignmentPropagatesAllocatorWhenPoccaTrue) {
    using Alloc = PropagatingTestAlloc<std::byte, /*Pocca=*/true, /*Pocma=*/false>;

    AllocCounter counterA{};
    AllocCounter counterB{};
    Alloc allocA{counterA};
    Alloc allocB{counterB};

    PoccaTruePocmaFalseDuck a{std::allocator_arg, allocA, BigWidget{41}};
    PoccaTruePocmaFalseDuck b{std::allocator_arg, allocB, BigWidget{42}};

    ASSERT_EQ(counterA.copies, 2); // One from construct, one from rebind
    ASSERT_EQ(counterA.allocs, 1);

    b = a;

    EXPECT_EQ(b.to_string(), "BigWidget(41)");
    EXPECT_EQ(a.to_string(), "BigWidget(41)");

    EXPECT_EQ(counterA.copies, 4);
    EXPECT_EQ(counterA.allocs, 2);
    EXPECT_EQ(counterB.deallocs, 1);
}

TEST(DuckAllocator, CopyAssignmentKeepsOwnAllocatorWhenPoccaFalse) {
    using Alloc = PropagatingTestAlloc<std::byte, /*Pocca=*/false, /*Pocma=*/false>;

    AllocCounter counterA{};
    AllocCounter counterB{};
    Alloc allocA{counterA};
    Alloc allocB{counterB};

    PoccaFalsePocmaFalseDuck a{std::allocator_arg, allocA, BigWidget{51}};
    PoccaFalsePocmaFalseDuck b{std::allocator_arg, allocB, BigWidget{52}};

    ASSERT_EQ(counterA.copies, 2); // One from construct, one from rebind
    ASSERT_EQ(counterB.allocs, 1); // from b's own construction only

    b = a;

    EXPECT_EQ(b.to_string(), "BigWidget(51)");

    EXPECT_EQ(counterA.copies, 2);

    // The copied-in value is still allocated through b's own (unchanged)
    // allocator: old storage freed, new storage allocated, both via B.
    EXPECT_EQ(counterB.allocs, 2);
    EXPECT_EQ(counterB.deallocs, 1);
}

TEST(DuckAllocator, MoveAssignmentPropagatesAllocatorWhenPocmaTrue) {
    using Alloc = PropagatingTestAlloc<std::byte, /*Pocca=*/false, /*Pocma=*/true>;

    AllocCounter counterA{};
    AllocCounter counterB{};
    Alloc allocA{counterA};
    Alloc allocB{counterB};

    PoccaFalsePocmaTrueDuck a{std::allocator_arg, allocA, BigWidget{61}};
    PoccaFalsePocmaTrueDuck b{std::allocator_arg, allocB, BigWidget{62}};

    ASSERT_EQ(counterA.allocs, 1);
    ASSERT_EQ(counterA.moves, 0);

    b = std::move(a);

    EXPECT_EQ(b.to_string(), "BigWidget(61)");

    EXPECT_EQ(counterA.moves, 1);

    // ...and because b now holds a's allocator, it's free to simply steal
    // a's already-allocated block rather than reallocate.
    EXPECT_EQ(counterA.allocs, 1);
}

TEST(DuckAllocator, MoveAssignmentKeepsOwnAllocatorWhenPocmaFalse) {
    using Alloc = PropagatingTestAlloc<std::byte, /*Pocca=*/false, /*Pocma=*/false>;

    AllocCounter counterA{};
    AllocCounter counterB{};
    Alloc allocA{counterA};
    Alloc allocB{counterB};

    PoccaFalsePocmaFalseDuck a{std::allocator_arg, allocA, BigWidget{71}};
    PoccaFalsePocmaFalseDuck b{std::allocator_arg, allocB, BigWidget{72}};

    ASSERT_EQ(counterA.moves, 0);
    ASSERT_EQ(counterB.allocs, 1);

    b = std::move(a);

    EXPECT_EQ(b.to_string(), "BigWidget(71)");

    // POCMA is false and the allocators are unequal, so b must keep its
    // own allocator rather than adopting a's.
    EXPECT_EQ(counterA.moves, 0);

    // Since b can't free memory it didn't allocate, it has to allocate its
    // own storage (via its own allocator) and move-construct the value
    // into it element-wise, instead of stealing a's block: old storage
    // freed, new storage allocated, both via B.
    EXPECT_EQ(counterB.allocs, 2);
    EXPECT_EQ(counterB.deallocs, 1);
}

TEST(DuckAllocator, CopyAssignmentWithPoccaLeavesNoLeaksOrDoubleFrees) {
    using Alloc = PropagatingTestAlloc<std::byte, true, false>;

    AllocCounter counterA{};
    AllocCounter counterB{};
    Alloc allocA{counterA};
    Alloc allocB{counterB};

    {
        PoccaTruePocmaFalseDuck a{std::allocator_arg, allocA, BigWidget{81}};
        PoccaTruePocmaFalseDuck b{std::allocator_arg, allocB, BigWidget{82}};
        b = a;
        b = a; // reassign again to also exercise the "already propagated" path
    }

    EXPECT_EQ(counterA.outstanding(), 0);
    EXPECT_EQ(counterB.outstanding(), 0);
}

TEST(DuckAllocator, MoveAssignmentWithPocmaLeavesNoLeaksOrDoubleFrees) {
    using Alloc = PropagatingTestAlloc<std::byte, false, true>;

    AllocCounter counterA{};
    AllocCounter counterB{};
    AllocCounter counterC{};
    Alloc allocA{counterA};
    Alloc allocB{counterB};
    Alloc allocC{counterC};

    {
        PoccaFalsePocmaTrueDuck a{std::allocator_arg, allocA, BigWidget{91}};
        PoccaFalsePocmaTrueDuck b{std::allocator_arg, allocB, BigWidget{92}};
        PoccaFalsePocmaTrueDuck c{std::allocator_arg, allocC, BigWidget{93}};
        b = std::move(a);
        b = std::move(c);
    }

    EXPECT_EQ(counterA.outstanding(), 0);
    EXPECT_EQ(counterB.outstanding(), 0);
    EXPECT_EQ(counterC.outstanding(), 0);
}

}  // namespace rjk_test