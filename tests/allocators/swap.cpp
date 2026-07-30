#include "fixtures.hpp"
#include <gtest/gtest.h>

namespace rjk_test {

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
    // POCS true: allocators travel with their data, so the equality
    // check inside the reused move-construction is trivially satisfied
    // on both hops -> zero reallocation, despite unequal allocators.
    EXPECT_EQ(counterA.allocs, 1);
    EXPECT_EQ(counterB.allocs, 1);
    EXPECT_EQ(counterA.deallocs, 0);
    EXPECT_EQ(counterB.deallocs, 0);

    // And each side now genuinely owns the OTHER's original allocator.
    EXPECT_TRUE(rjk::get_allocator(a) == allocB);
    EXPECT_TRUE(rjk::get_allocator(b) == allocA);
}

TEST(DuckAllocatorSwap, SwapWithPocma) {
    AllocCounter counterA{};
    AllocCounter counterB{};
    PocmaAlloc allocA{counterA};
    PocmaAlloc allocB{counterB};

    AllocDuck<Pocma> a{std::allocator_arg, allocA, BigWidget{71}};
    AllocDuck<Pocma> b{std::allocator_arg, allocB, BigWidget{72}};

    rjk::swap(a, b);

    EXPECT_EQ(a.to_string(), "BigWidget(72)");
    EXPECT_EQ(b.to_string(), "BigWidget(71)");
    // POCMA true must NOT leak into swap's behavior: allocators stay
    // put (as in the POCS-false case above), proving swap consults
    // propagate_on_container_swap independently of POCMA.
    EXPECT_TRUE(rjk::get_allocator(a) == allocA);
    EXPECT_TRUE(rjk::get_allocator(b) == allocB);
}

TEST(DuckAllocatorSwap, SwapNoLeaks) {
    AllocCounter counterA{};
    AllocCounter counterB{};
    NoneAlloc allocA{counterA};
    NoneAlloc allocB{counterB};

    {
        AllocDuck<None> a{std::allocator_arg, allocA, BigWidget{81}};
        AllocDuck<None> b{std::allocator_arg, allocB, Widget{82}};
        rjk::swap(a, b);
        rjk::swap(a, b);
    }

    EXPECT_EQ(counterA.outstanding(), 0);
    EXPECT_EQ(counterB.outstanding(), 0);
}

}  // namespace rjk_test