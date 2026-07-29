#include "fixtures.hpp"

#include <gtest/gtest.h>

namespace rjk_test {

TEST(AllocConstructor, CopyConstructorUnequal) {
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

TEST(AllocConstructor, MoveConstructorUnequal) {
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

TEST(AllocConstructor, MoveConstructorEqual) {
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

}