#include "fixtures.hpp"
#include <gtest/gtest.h>

namespace rjk_test {
namespace {

TEST(DuckAllocatorSwap, SwapHeapEqualAllocs) {
    AllocCounter counter{};
    NoneAlloc allocA{counter};
    NoneAlloc allocB{counter};

    AllocDuck<None> a{std::allocator_arg, allocA, BigWidget{1}};
    AllocDuck<None> b{std::allocator_arg, allocB, BigWidget{2}};
    ASSERT_EQ(counter.allocs, 2);

    rjk::swap(a, b);

    EXPECT_EQ(a.to_string(), "BigWidget(2)");
    EXPECT_EQ(b.to_string(), "BigWidget(1)");
    
    EXPECT_EQ(counter.allocs, 2);
    EXPECT_EQ(counter.deallocs, 0);
}

TEST(DuckAllocatorSwap, SwapSBOWithHeap) {
    AllocCounter counter{};
    NoneAlloc allocA{counter};
    NoneAlloc allocB{counter};

    AllocDuck<None> a{std::allocator_arg, allocA, Widget{10}};   // SBO
    AllocDuck<None> b{std::allocator_arg, allocB, BigWidget{20}}; // heap

    rjk::swap(a, b);

    EXPECT_EQ(a.to_string(), "BigWidget(20)");
    EXPECT_EQ(b.to_string(), "Widget(10)");
}

TEST(DuckAllocatorSwap, SwapTwoSbo) {
    AllocCounter counter{};
    AlwaysEqualAlloc alloc{counter};

    AllocDuck<AlwaysEqual> a{std::allocator_arg, alloc, Widget{11}};
    AllocDuck<AlwaysEqual> b{std::allocator_arg, alloc, Widget{22}};

    rjk::swap(a, b);

    EXPECT_EQ(a.to_string(), "Widget(22)");
    EXPECT_EQ(b.to_string(), "Widget(11)");
    EXPECT_EQ(counter.allocs, 0);
}

// Two SBO values, genuinely different (non-always-equal, non-POCS)
// allocators. Since neither side ever owns a heap block, allocator
// inequality should be a non-issue here — nothing to steal, nothing to
// reallocate, and both allocators are left exactly where they started.
TEST(DuckAllocatorSwap, SwapTwoSboUnequalNonPocsAllocators) {
    AllocCounter counterA{};
    AllocCounter counterB{};
    NoneAlloc allocA{counterA};
    NoneAlloc allocB{counterB};

    AllocDuck<None> a{std::allocator_arg, allocA, Widget{12}};
    AllocDuck<None> b{std::allocator_arg, allocB, Widget{23}};

    rjk::swap(a, b);

    EXPECT_EQ(a.to_string(), "Widget(23)");
    EXPECT_EQ(b.to_string(), "Widget(12)");
    EXPECT_EQ(counterA.allocs, 0);
    EXPECT_EQ(counterB.allocs, 0);
    EXPECT_TRUE(rjk::get_allocator(a) == allocA);
    EXPECT_TRUE(rjk::get_allocator(b) == allocB);
}

TEST(DuckAllocatorSwap, SelfSwap) {
    AllocCounter counter{};
    AlwaysEqualAlloc alloc{counter};

    AllocDuck<AlwaysEqual> a{std::allocator_arg, alloc, BigWidget{33}};
    ASSERT_EQ(counter.allocs, 1);

    rjk::swap(a, a);

    EXPECT_EQ(a.to_string(), "BigWidget(33)");
    EXPECT_EQ(counter.allocs, 1);
    EXPECT_EQ(counter.deallocs, 0);
}

TEST(DuckAllocatorSwap, AlwaysEqualSwap) {
    AllocCounter counterA{};
    AllocCounter counterB{};
    AlwaysEqualAlloc allocA{counterA};
    AlwaysEqualAlloc allocB{counterB};

    AllocDuck<AlwaysEqual> a{std::allocator_arg, allocA, BigWidget{41}};
    AllocDuck<AlwaysEqual> b{std::allocator_arg, allocB, BigWidget{42}};
    ASSERT_EQ(counterA.allocs, 1);
    ASSERT_EQ(counterB.allocs, 1);

    rjk::swap(a, b);

    EXPECT_EQ(a.to_string(), "BigWidget(42)");
    EXPECT_EQ(b.to_string(), "BigWidget(41)");

    EXPECT_EQ(counterA.allocs, 1);
    EXPECT_EQ(counterB.allocs, 1);
}

TEST(DuckAllocatorSwap, PocsSwap) {
    AllocCounter counterA{};
    AllocCounter counterB{};
    PocsAlloc allocA{counterA};
    PocsAlloc allocB{counterB};

    AllocDuck<Pocs> a{std::allocator_arg, allocA, BigWidget{51}};
    AllocDuck<Pocs> b{std::allocator_arg, allocB, BigWidget{52}};
    ASSERT_EQ(counterA.allocs, 1);
    ASSERT_EQ(counterB.allocs, 1);

    rjk::swap(a, b);

    EXPECT_EQ(a.to_string(), "BigWidget(52)");
    EXPECT_EQ(b.to_string(), "BigWidget(51)");

    EXPECT_EQ(counterA.allocs, 1);
    EXPECT_EQ(counterB.allocs, 1);
    EXPECT_EQ(counterA.deallocs, 0);
    EXPECT_EQ(counterB.deallocs, 0);

    EXPECT_TRUE(rjk::get_allocator(a) == allocB);
    EXPECT_TRUE(rjk::get_allocator(b) == allocA);
}

// propagate_on_container_move_assignment doesn't make swap propagate: the
// allocators stay where they were. Unequal non-POCS allocators are only valid
// to swap when both values are SBO-resident, so the values are Widgets.
TEST(DuckAllocatorSwap, SwapWithPocma) {
    AllocCounter counterA{};
    AllocCounter counterB{};
    PocmaAlloc allocA{counterA};
    PocmaAlloc allocB{counterB};

    AllocDuck<Pocma> a{std::allocator_arg, allocA, Widget{71}};
    AllocDuck<Pocma> b{std::allocator_arg, allocB, Widget{72}};

    rjk::swap(a, b);

    EXPECT_EQ(a.to_string(), "Widget(72)");
    EXPECT_EQ(b.to_string(), "Widget(71)");

    EXPECT_TRUE(rjk::get_allocator(a) == allocA);
    EXPECT_TRUE(rjk::get_allocator(b) == allocB);
    EXPECT_EQ(counterA.allocs, 0);
    EXPECT_EQ(counterB.allocs, 0);
}

// Swapping heap and SBO values back and forth leaks nothing. The allocators
// compare equal, as required for non-POCS swaps involving heap storage.
TEST(DuckAllocatorSwap, SwapNoLeaks) {
    AllocCounter counter{};
    NoneAlloc allocA{counter};
    NoneAlloc allocB{counter};

    {
        AllocDuck<None> a{std::allocator_arg, allocA, BigWidget{81}};
        AllocDuck<None> b{std::allocator_arg, allocB, Widget{82}};
        rjk::swap(a, b);
        rjk::swap(a, b);

        EXPECT_EQ(a.to_string(), "BigWidget(81)");
        EXPECT_EQ(b.to_string(), "Widget(82)");
    }

    EXPECT_EQ(counter.allocs, 1);
    EXPECT_EQ(counter.outstanding(), 0);
}

// Swapping heap storage between unequal, non-propagating allocators is
// undefined behaviour (as for standard containers); storage::swap asserts.
#if !defined(NDEBUG) && GTEST_HAS_DEATH_TEST
TEST(DuckAllocatorSwap, SwapUnequalNonPocsHeapAsserts) {
    AllocCounter counterA{};
    AllocCounter counterB{};
    NoneAlloc allocA{counterA};
    NoneAlloc allocB{counterB};

    AllocDuck<None> a{std::allocator_arg, allocA, BigWidget{91}};
    AllocDuck<None> b{std::allocator_arg, allocB, BigWidget{92}};

    ASSERT_DEATH(rjk::swap(a, b),
                 "allocators must compare equal unless");
}
#endif

} // namespace
}  // namespace rjk_test
