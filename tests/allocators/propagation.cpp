#include "fixtures.hpp"

#include <gtest/gtest.h>

namespace rjk_test {

TEST(AllocatorPropagation, CopyAssignmentPocca) {
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

TEST(AllocatorPropagation, CopyAssignmentNoPocca) {
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

TEST(AllocatorPropagation, MoveAssignmentPocma) {
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

TEST(AllocatorPropagation, MoveAssignmentNoPocma) {
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

TEST(AllocatorPropagation, CopyAssignmentPoccaLeaks) {
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

TEST(AllocatorPropagation, MoveAssignmentPocmaLeaks) {
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

}