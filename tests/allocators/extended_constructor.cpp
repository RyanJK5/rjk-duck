#include "fixtures.hpp"

#include <gtest/gtest.h>

namespace rjk_test {

TEST(AllocConstructor, CopyConstructorUnequal) {
    AllocCounter counterSrc{};
    AllocCounter counterDst{};
    NoneAlloc allocSrc{counterSrc};
    NoneAlloc allocDst{counterDst};

    AllocDuck<None> original{std::allocator_arg, allocSrc, BigWidget{31}};
    ASSERT_EQ(counterSrc.allocs, 1);

    // Allocator-extended copy: the copy's storage comes from allocDst
    // (the one explicitly supplied), not from original's allocator, and
    // original is left completely untouched.
    AllocDuck<None> copy{std::allocator_arg, allocDst, original};

    EXPECT_EQ(copy.to_string(), "BigWidget(31)");
    EXPECT_EQ(counterDst.allocs, 1);
    EXPECT_EQ(counterSrc.allocs, 1);
    EXPECT_EQ(counterSrc.deallocs, 0);
}

TEST(AllocConstructor, MoveConstructorUnequal) {
    AllocCounter counterSrc{};
    AllocCounter counterDst{};
    NoneAlloc allocSrc{counterSrc};
    NoneAlloc allocDst{counterDst};

    AllocDuck<None> original{std::allocator_arg, allocSrc, BigWidget{32}};
    ASSERT_EQ(counterSrc.allocs, 1);

    AllocDuck<None> moved{std::allocator_arg, allocDst, std::move(original)};

    EXPECT_EQ(moved.to_string(), "BigWidget(32)");
    EXPECT_EQ(counterDst.allocs, 1);
}

TEST(AllocConstructor, MoveConstructorEqual) {
    AllocCounter counter{};
    NoneAlloc allocA{counter};
    NoneAlloc allocB{counter}; //

    AllocDuck<None> original{std::allocator_arg, allocA, BigWidget{33}};
    ASSERT_EQ(counter.allocs, 1);

    // allocA == allocB (same underlying counter), so this should transplant
    // the existing heap block instead of allocating again.
    AllocDuck<None> moved{std::allocator_arg, allocB, std::move(original)};

    EXPECT_EQ(moved.to_string(), "BigWidget(33)");
    EXPECT_EQ(counter.allocs, 1);
    EXPECT_EQ(counter.deallocs, 0);
}

TEST(AllocConstructor, MoveConstructorSbo) {
    AllocCounter counterSrc{};
    AllocCounter counterDst{};
    NoneAlloc allocSrc{counterSrc};
    NoneAlloc allocDst{counterDst};

    AllocDuck<None> original{std::allocator_arg, allocSrc, Widget{7}};
    ASSERT_EQ(counterSrc.allocs, 0); // Widget fits SBO, no heap involved

    AllocDuck<None> moved{std::allocator_arg, allocDst, std::move(original)};

    EXPECT_EQ(moved.to_string(), "Widget(7)");
    // SBO moves happen in-place unconditionally; allocator inequality is
    // irrelevant since there's nothing to steal or reallocate.
    EXPECT_EQ(counterSrc.allocs, 0);
    EXPECT_EQ(counterDst.allocs, 0);
}

}