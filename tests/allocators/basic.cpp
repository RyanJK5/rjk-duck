#include "rjk/duck.hpp"
#include "fixtures.hpp"

#include <gtest/gtest.h>

namespace rjk_test {

TEST(BasicAllocator, AllocatorConstructor) {
    AllocCounter counter{};
    AlwaysEqualAlloc alloc{counter};

    AllocDuck<AlwaysEqual> d{std::allocator_arg, alloc, BigWidget{1}};

    EXPECT_EQ(d.to_string(), "BigWidget(1)");
    EXPECT_EQ(counter.allocs, 1);
    EXPECT_EQ(counter.deallocs, 0);
}

TEST(BasicAllocator, AllocatorDestructor) {
    AllocCounter counter{};
    AlwaysEqualAlloc alloc{counter};

    {
        AllocDuck<AlwaysEqual> d{std::allocator_arg, alloc, BigWidget{2}};
        EXPECT_EQ(counter.allocs, 1);
        EXPECT_EQ(counter.deallocs, 0);
    }

    EXPECT_EQ(counter.deallocs, 1);
    EXPECT_EQ(counter.outstanding(), 0);
}

TEST(BasicAllocator, ReassigningHeapAlloc) {
    AllocCounter counter{};
    AlwaysEqualAlloc alloc{counter};

    AllocDuck<AlwaysEqual> d{std::allocator_arg, alloc, BigWidget{3}};
    ASSERT_EQ(counter.allocs, 1);
    ASSERT_EQ(counter.deallocs, 0);

    d = BigWidget{4};

    EXPECT_EQ(d.to_string(), "BigWidget(4)");
    EXPECT_EQ(counter.allocs, 2);
    EXPECT_EQ(counter.deallocs, 1);
}

TEST(BasicAllocator, ReassigningToSbo) {
    AllocCounter counter{};
    AlwaysEqualAlloc alloc{counter};

    AllocDuck<AlwaysEqual> d{std::allocator_arg, alloc, BigWidget{5}};
    ASSERT_EQ(counter.allocs, 1);

    d = Widget{99};

    EXPECT_EQ(d.to_string(), "Widget(99)");
    EXPECT_EQ(counter.allocs, 1);
    EXPECT_EQ(counter.deallocs, 1);
}

TEST(BasicAllocator, Emplace) {
    AllocCounter counter{};
    AlwaysEqualAlloc alloc{counter};

    AllocDuck<AlwaysEqual> d{std::allocator_arg, alloc, Widget{1}};
    ASSERT_EQ(counter.allocs, 0);

    rjk::emplace<BigWidget>(d, 7);

    EXPECT_EQ(d.to_string(), "BigWidget(7)");
    EXPECT_EQ(counter.allocs, 1);
    EXPECT_EQ(counter.deallocs, 0);
}

TEST(BasicAllocator, CopyConstruct) {
    AllocCounter counter{};
    AlwaysEqualAlloc alloc{counter};

    AllocDuck<AlwaysEqual> original{std::allocator_arg, alloc, BigWidget{11}};
    ASSERT_EQ(counter.allocs, 1);

    AllocDuck   <AlwaysEqual> copy{original};

    EXPECT_EQ(copy.to_string(), "BigWidget(11)");
    EXPECT_GE(counter.copies, 1);
    EXPECT_EQ(counter.allocs, 2);   // fresh storage for the copy's BigWidget
    EXPECT_EQ(counter.deallocs, 0); // original is untouched
}

TEST(BasicAllocator, MoveConstruct) {
    AllocCounter counter{};
    AlwaysEqualAlloc alloc{counter};

    AllocDuck<AlwaysEqual> original{std::allocator_arg, alloc, BigWidget{22}};
    ASSERT_EQ(counter.allocs, 1);

    AllocDuck<AlwaysEqual> moved{std::move(original)};

    EXPECT_EQ(moved.to_string(), "BigWidget(22)");
    // Moving transplants the same heap block; no new alloc/dealloc.
    EXPECT_EQ(counter.allocs, 1);
    EXPECT_EQ(counter.deallocs, 0);
}

TEST(BasicAllocator, EmplaceOnEmptyDuck) {
    AllocCounter counter{};
    AlwaysEqualAlloc alloc{counter};

    AllocDuck<AlwaysEqual> source{std::allocator_arg, alloc, BigWidget{121}};
    AllocDuck<AlwaysEqual> moved_from{std::move(source)};
    // `source` is now valueless (moved-from): get_vtable() == nullptr.

    rjk::emplace<Widget>(source, 55);

    EXPECT_EQ(source.to_string(), "Widget(55)");
}

}