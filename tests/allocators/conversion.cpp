#include "fixtures.hpp"
#include "rjk/duck.hpp"

#include <gtest/gtest.h>

namespace rjk_test {

template <typename Perf>
using ReorderedDuck = rjk::duck<Perf, Stringify, rjk::copyable>;

TEST(DuckAllocatorConversion, PermutationAllocConstructor) {
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

TEST(DuckAllocatorConversion, PermutationUnequalAllocConstructor) {

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

TEST(DuckAllocatorConversion, PermutationMoveConstructor) {
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

// SBO variant of the permutation-conversion path: reordering the perf
// option in the template parameter list, plus an allocator swap, must
// stay a pure no-allocation operation when the value fits in-place.
TEST(DuckAllocatorConversion, PermutationMoveConstructorSbo) {
    AllocCounter counterSrc{};
    AllocCounter counterDst{};
    NoneAlloc allocSrc{counterSrc};
    NoneAlloc allocDst{counterDst};

    AllocDuck<None> original{std::allocator_arg, allocSrc, Widget{214}};
    ASSERT_EQ(counterSrc.allocs, 0);

    ReorderedDuck<None> moved{std::allocator_arg, allocDst, std::move(original)};

    EXPECT_EQ(moved.to_string(), "Widget(214)");
    EXPECT_EQ(counterSrc.allocs, 0);
    EXPECT_EQ(counterDst.allocs, 0);
}

TEST(DuckAllocatorConversion, NarrowingSameAlloc) {
    AllocCounter counter{};
    NoneAlloc alloc{counter};

    rjk::duck<Stringify, rjk::copyable, None> original{std::allocator_arg, alloc, BigWidget{231}};
    ASSERT_EQ(counter.allocs, 1);

    auto narrowed = rjk::make_narrowed<None>(std::move(original));

    EXPECT_TRUE(rjk::get_allocator(narrowed) == alloc);
    EXPECT_EQ(counter.allocs, 1);
    EXPECT_EQ(counter.deallocs, 0);
}

TEST(DuckAllocatorConversion, NarrowingDifferentAlloc) {
    AllocCounter counter{};
    NoneAlloc alloc{counter};

    rjk::duck<Stringify, rjk::copyable, None> original{std::allocator_arg, alloc, BigWidget{232}};
    ASSERT_EQ(counter.allocs, 1);

    auto narrowed = rjk::make_narrowed<Stringify>(std::move(original));

    EXPECT_EQ(narrowed.to_string(), "BigWidget(232)");
    EXPECT_EQ(counter.deallocs, 1);
}

TEST(DuckAllocatorConversion, NarrowingCopyAlloc) {
    struct CopyableStringify : Stringify, rjk::copyable {};

    AllocCounter counter{};
    NoneAlloc alloc{counter};

    rjk::duck<CopyableStringify, None> original{std::allocator_arg, alloc, BigWidget{233}};
    ASSERT_EQ(counter.allocs, 1);

    auto narrowed = rjk::make_narrowed<CopyableStringify>(original);

    EXPECT_EQ(narrowed.to_string(), "BigWidget(233)");
    EXPECT_EQ(original.to_string(), "BigWidget(233)"); // untouched
    EXPECT_EQ(counter.allocs, 1); // no second copy through original's allocator
    EXPECT_EQ(counter.deallocs, 0);
}

TEST(DuckAllocatorConversion, NarrowingExplicitAlloc) {
    AllocCounter counterSrc{};
    AllocCounter counterDst{};
    NoneAlloc allocSrc{counterSrc};
    NoneAlloc allocDst{counterDst};

    rjk::duck<Stringify, rjk::copyable, None> original{std::allocator_arg, allocSrc, BigWidget{234}};
    ASSERT_EQ(counterSrc.allocs, 1);

    auto narrowed = rjk::make_narrowed<None>(std::allocator_arg, allocDst, std::move(original));

    EXPECT_EQ(counterDst.allocs, 1);
    EXPECT_EQ(counterSrc.deallocs, 1);
}

TEST(DuckAllocatorConversion, NarrowingSbo) {
    AllocCounter counter{};
    NoneAlloc alloc{counter};

    rjk::duck<Stringify, rjk::copyable, None> original{std::allocator_arg, alloc, Widget{235}};
    ASSERT_EQ(counter.allocs, 0);

    auto narrowed = rjk::make_narrowed<None>(std::move(original));

    EXPECT_TRUE(rjk::get_allocator(narrowed) == alloc);
    EXPECT_EQ(counter.allocs, 0);
    EXPECT_EQ(counter.deallocs, 0);
}

}