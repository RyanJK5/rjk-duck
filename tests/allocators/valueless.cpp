#include "fixtures.hpp"

#include <gtest/gtest.h>

namespace rjk_test {

TEST(DuckAllocatorValueless, MoveAssignFromValuelessSource) {
    AllocCounter counterA{};
    AllocCounter counterB{};
    NoneAlloc allocA{counterA};
    NoneAlloc allocB{counterB};

    AllocDuck<None> source{std::allocator_arg, allocA, BigWidget{401}};
    AllocDuck<None> _{std::move(source)}; // `source` now valueless
    AllocDuck<None> b{std::allocator_arg, allocB, BigWidget{402}};
    ASSERT_EQ(counterB.allocs, 1);

    b = std::move(source); // assigning FROM a valueless duck

    EXPECT_EQ(counterB.deallocs, 1); // b's own prior value must still be released
}

TEST(DuckAllocatorValueless, CopyAssignFromValuelessSource) {
    AllocCounter counterA{};
    AllocCounter counterB{};
    NoneAlloc allocA{counterA};
    NoneAlloc allocB{counterB};

    AllocDuck<None> source{std::allocator_arg, allocA, BigWidget{405}};
    AllocDuck<None> _{std::move(source)};
    AllocDuck<None> b{std::allocator_arg, allocB, BigWidget{406}};
    ASSERT_EQ(counterB.allocs, 1);

    b = source; // copy-assigning FROM a valueless duck

    EXPECT_EQ(counterB.deallocs, 1);
}

TEST(DuckAllocatorValueless, MoveAssignIntoValuelessDestination) {
    AllocCounter counterA{};
    AllocCounter counterB{};
    PocmaAlloc allocA{counterA};
    PocmaAlloc allocB{counterB};

    AllocDuck<Pocma> a{std::allocator_arg, allocA, BigWidget{403}};
    AllocDuck<Pocma> source{std::allocator_arg, allocB, BigWidget{404}};
    AllocDuck<Pocma> keepAlive{std::move(source)};

    ASSERT_TRUE(rjk::valueless_after_move(source));

    source = std::move(a);

    EXPECT_EQ(source.to_string(), "BigWidget(403)");
    EXPECT_EQ(counterA.allocs, 1);
    EXPECT_EQ(counterA.deallocs, 0); // the incoming block was transplanted, not freed
}

TEST(DuckAllocatorValueless, CopyAssignIntoValuelessDestination) {
    AllocCounter counterA{};
    AllocCounter counterB{};
    NoneAlloc allocA{counterA};
    NoneAlloc allocB{counterB};

    AllocDuck<None> a{std::allocator_arg, allocA, BigWidget{408}};
    AllocDuck<None> source{std::allocator_arg, allocB, BigWidget{409}};
    AllocDuck<None> keepAlive{std::move(source)};
    // `source` is now valueless.

    source = a;

    EXPECT_EQ(source.to_string(), "BigWidget(408)");
    EXPECT_EQ(a.to_string(), "BigWidget(408)"); // a is untouched by a copy
}

}  // namespace rjk_test