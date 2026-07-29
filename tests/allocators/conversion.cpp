#include "fixtures.hpp"
#include "rjk/duck.hpp"

#include <gtest/gtest.h>

namespace rjk_test {

template <typename Perf>
using ReorderedDuck = rjk::duck<Perf, Stringify, rjk::copyable>;

TEST(DuckAllocatorConversion, PermutationConstructorUsesSuppliedAllocatorNotSources) {
    AllocCounter counterSrc{};
    AllocCounter counterDst{};
    AlwaysEqualAlloc allocSrc{counterSrc};
    AlwaysEqualAlloc allocDst{counterDst};

    AllocDuck<AlwaysEqual> original{std::allocator_arg, allocSrc, BigWidget{211}};
    ASSERT_EQ(counterSrc.allocs, 1);

    ReorderedDuck<AlwaysEqual> reordered{std::allocator_arg, allocDst, std::move(original)};

    EXPECT_EQ(reordered.to_string(), "BigWidget(211)");
    EXPECT_EQ(rjk::get_allocator(reordered), allocDst);
}

TEST(DuckAllocatorConversion, PermutationMoveWithUnequalAllocatorReallocates) {

    AllocCounter counterSrc{};
    AllocCounter counterDst{};
    NoneAlloc allocSrc{counterSrc};
    NoneAlloc allocDst{counterDst};

    AllocDuck<None> original{std::allocator_arg, allocSrc, BigWidget{212}};
    ASSERT_EQ(counterSrc.allocs, 1);

    ReorderedDuck<None> moved{std::allocator_arg, allocDst, std::move(original)};

    EXPECT_EQ(moved.to_string(), "BigWidget(212)");
    EXPECT_EQ(counterDst.allocs, 1);
}

TEST(DuckAllocatorConversion, PermutationMoveWithEqualAllocatorSteals) {
    AllocCounter counter{};
    NoneAlloc allocA{counter};
    NoneAlloc allocB{counter};

    AllocDuck<None> original{std::allocator_arg, allocA, BigWidget{213}};
    ASSERT_EQ(counter.allocs, 1);

    ReorderedDuck<None> moved{std::allocator_arg, allocB, std::move(original)};

    EXPECT_EQ(moved.to_string(), "BigWidget(213)");
    EXPECT_EQ(counter.allocs, 1);
    EXPECT_EQ(counter.deallocs, 0);
}

}