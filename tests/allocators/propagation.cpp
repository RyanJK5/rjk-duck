#include "fixtures.hpp"

#include <gtest/gtest.h>

namespace rjk_test {

TEST(AllocatorPropagation, CopyAssignmentPocca) {
    AllocCounter counterA{};
    AllocCounter counterB{};
    PoccaAlloc allocA{counterA};
    PoccaAlloc allocB{counterB};

    AllocDuck<Pocca> a{std::allocator_arg, allocA, BigWidget{41}};
    AllocDuck<Pocca> b{std::allocator_arg, allocB, BigWidget{42}};

    ASSERT_EQ(counterA.copies, 2); // One from construct, one from rebind
    ASSERT_EQ(counterA.allocs, 1);

    b = a;

    EXPECT_EQ(b.to_string(), "BigWidget(41)");
    EXPECT_EQ(a.to_string(), "BigWidget(41)");

    EXPECT_EQ(counterA.copies, 4);
    EXPECT_EQ(counterA.allocs, 2);
    EXPECT_EQ(counterB.deallocs, 1);

    // POCCA true: b must now genuinely hold a's allocator.
    EXPECT_TRUE(rjk::get_allocator(b) == allocA);
}

TEST(AllocatorPropagation, CopyAssignmentNoPocca) {
    AllocCounter counterA{};
    AllocCounter counterB{};
    NoneAlloc allocA{counterA};
    NoneAlloc allocB{counterB};

    AllocDuck<None> a{std::allocator_arg, allocA, BigWidget{51}};
    AllocDuck<None> b{std::allocator_arg, allocB, BigWidget{52}};

    ASSERT_EQ(counterA.copies, 2); // One from construct, one from rebind
    ASSERT_EQ(counterB.allocs, 1); // from b's own construction only

    b = a;

    EXPECT_EQ(b.to_string(), "BigWidget(51)");

    EXPECT_EQ(counterA.copies, 2);

    // The copied-in value is still allocated through b's own (unchanged)
    // allocator: old storage freed, new storage allocated, both via B.
    EXPECT_EQ(counterB.allocs, 2);
    EXPECT_EQ(counterB.deallocs, 1);

    // POCCA false: b must keep its own allocator identity, not adopt a's.
    EXPECT_TRUE(rjk::get_allocator(b) == allocB);
    EXPECT_FALSE(rjk::get_allocator(b) == allocA);
}

TEST(AllocatorPropagation, MoveAssignmentPocma) {
    AllocCounter counterA{};
    AllocCounter counterB{};
    PocmaAlloc allocA{counterA};
    PocmaAlloc allocB{counterB};

    AllocDuck<Pocma> a{std::allocator_arg, allocA, BigWidget{61}};
    AllocDuck<Pocma> b{std::allocator_arg, allocB, BigWidget{62}};

    ASSERT_EQ(counterA.allocs, 1);
    ASSERT_EQ(counterA.moves, 0);

    b = std::move(a);

    EXPECT_EQ(b.to_string(), "BigWidget(61)");

    EXPECT_EQ(counterA.moves, 1);

    // ...and because b now holds a's allocator, it's free to simply steal
    // a's already-allocated block rather than reallocate.
    EXPECT_EQ(counterA.allocs, 1);

    EXPECT_TRUE(rjk::get_allocator(b) == allocA);
}

TEST(AllocatorPropagation, MoveAssignmentNoPocma) {
    AllocCounter counterA{};
    AllocCounter counterB{};
    NoneAlloc allocA{counterA};
    NoneAlloc allocB{counterB};

    AllocDuck<None> a{std::allocator_arg, allocA, BigWidget{71}};
    AllocDuck<None> b{std::allocator_arg, allocB, BigWidget{72}};

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

    EXPECT_TRUE(rjk::get_allocator(b) == allocB);
    EXPECT_FALSE(rjk::get_allocator(b) == allocA);
}

TEST(AllocatorPropagation, CopyAssignmentPoccaLeaks) {
    AllocCounter counterA{};
    AllocCounter counterB{};
    PoccaAlloc allocA{counterA};
    PoccaAlloc allocB{counterB};

    {
        AllocDuck<Pocca> a{std::allocator_arg, allocA, BigWidget{81}};
        AllocDuck<Pocca> b{std::allocator_arg, allocB, BigWidget{82}};
        b = a;
        b = a; // reassign again to also exercise the "already propagated" path
    }

    EXPECT_EQ(counterA.outstanding(), 0);
    EXPECT_EQ(counterB.outstanding(), 0);
}

TEST(AllocatorPropagation, MoveAssignmentPocmaLeaks) {
    AllocCounter counterA{};
    AllocCounter counterB{};
    AllocCounter counterC{};
    PocmaAlloc allocA{counterA};
    PocmaAlloc allocB{counterB};
    PocmaAlloc allocC{counterC};

    {
        AllocDuck<Pocma> a{std::allocator_arg, allocA, BigWidget{91}};
        AllocDuck<Pocma> b{std::allocator_arg, allocB, BigWidget{92}};
        AllocDuck<Pocma> c{std::allocator_arg, allocC, BigWidget{93}};
        b = std::move(a);
        b = std::move(c);
    }

    EXPECT_EQ(counterA.outstanding(), 0);
    EXPECT_EQ(counterB.outstanding(), 0);
    EXPECT_EQ(counterC.outstanding(), 0);
}

TEST(AllocatorPropagation, SelfMoveAssignment) {
    AllocCounter counterA{};
    PocmaAlloc allocA{counterA};

    AllocDuck<Pocma> a{std::allocator_arg, allocA, BigWidget{101}};
    ASSERT_EQ(counterA.allocs, 1);

    // Route through a reference to avoid -Wself-move on the literal `a = std::move(a)`.
    auto& self_ref = a;
    a = std::move(self_ref);

    EXPECT_EQ(a.to_string(), "BigWidget(101)");
    EXPECT_EQ(counterA.allocs, 1);
    EXPECT_EQ(counterA.deallocs, 0);
    EXPECT_EQ(counterA.moves, 0);
}

TEST(AllocatorPropagation, SelfCopyAssignment) {
    AllocCounter counterA{};
    PoccaAlloc allocA{counterA};

    AllocDuck<Pocca> a{std::allocator_arg, allocA, BigWidget{102}};
    ASSERT_EQ(counterA.allocs, 1);
    ASSERT_EQ(counterA.copies, 2); // construct + rebind, per earlier discussion

    const auto& self_ref = a;
    a = self_ref;

    EXPECT_EQ(a.to_string(), "BigWidget(102)");
    EXPECT_EQ(counterA.allocs, 1);
    EXPECT_EQ(counterA.deallocs, 0);
    EXPECT_EQ(counterA.copies, 2);
}

// --- SBO variants of the two self-assignment tests above: no heap block
// exists at all, so these confirm self-assignment detection doesn't rely
// on (and isn't broken by) the presence of allocated storage. ---

TEST(AllocatorPropagation, SelfMoveAssignmentSbo) {
    AllocCounter counterA{};
    PocmaAlloc allocA{counterA};

    AllocDuck<Pocma> a{std::allocator_arg, allocA, Widget{103}};
    ASSERT_EQ(counterA.allocs, 0);

    auto& self_ref = a;
    a = std::move(self_ref);

    EXPECT_EQ(a.to_string(), "Widget(103)");
    EXPECT_EQ(counterA.allocs, 0);
    EXPECT_EQ(counterA.deallocs, 0);
}

TEST(AllocatorPropagation, SelfCopyAssignmentSbo) {
    AllocCounter counterA{};
    PoccaAlloc allocA{counterA};

    AllocDuck<Pocca> a{std::allocator_arg, allocA, Widget{104}};
    ASSERT_EQ(counterA.allocs, 0);

    const auto& self_ref = a;
    a = self_ref;

    EXPECT_EQ(a.to_string(), "Widget(104)");
    EXPECT_EQ(counterA.allocs, 0);
    EXPECT_EQ(counterA.deallocs, 0);
}

// --- SBO variants of the Pocca/Pocma assignment tests: confirm allocator
// *identity* still propagates correctly per POCCA/POCMA even when there's
// no heap block being fought over. ---

TEST(AllocatorPropagation, CopyAssignmentPoccaSboValue) {
    AllocCounter counterA{};
    AllocCounter counterB{};
    PoccaAlloc allocA{counterA};
    PoccaAlloc allocB{counterB};

    AllocDuck<Pocca> a{std::allocator_arg, allocA, Widget{43}};
    AllocDuck<Pocca> b{std::allocator_arg, allocB, Widget{44}};

    b = a;

    EXPECT_EQ(b.to_string(), "Widget(43)");
    EXPECT_TRUE(rjk::get_allocator(b) == allocA);
}

TEST(AllocatorPropagation, MoveAssignmentPocmaSboValue) {
    AllocCounter counterA{};
    AllocCounter counterB{};
    PocmaAlloc allocA{counterA};
    PocmaAlloc allocB{counterB};

    AllocDuck<Pocma> a{std::allocator_arg, allocA, Widget{63}};
    AllocDuck<Pocma> b{std::allocator_arg, allocB, Widget{64}};

    b = std::move(a);

    EXPECT_EQ(b.to_string(), "Widget(63)");
    EXPECT_TRUE(rjk::get_allocator(b) == allocA);
}

TEST(AllocatorPropagation, AlwaysEqual) {
    AllocCounter counterA{};
    AllocCounter counterB{};
    AlwaysEqualAlloc allocA{counterA};
    AlwaysEqualAlloc allocB{counterB};

    AllocDuck<AlwaysEqual> a{std::allocator_arg, allocA, BigWidget{111}};
    AllocDuck<AlwaysEqual> b{std::allocator_arg, allocB, BigWidget{112}};
    ASSERT_EQ(counterA.allocs, 1);
    ASSERT_EQ(counterB.allocs, 1);

    b = std::move(a);

    EXPECT_EQ(b.to_string(), "BigWidget(111)");
    // TestAlloc::is_always_equal is true, so the compile-time fast path
    // steals a's block directly — no runtime comparison of counterA vs
    // counterB, and no reallocation through b, even though the counters
    // genuinely differ.
    EXPECT_EQ(counterA.allocs, 1);
    EXPECT_EQ(counterB.allocs, 1);
    EXPECT_EQ(counterB.deallocs, 1); // b's original BigWidget(112) freed
}

}