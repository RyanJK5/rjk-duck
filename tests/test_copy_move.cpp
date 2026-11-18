#include "rjk/duck.hpp"

#include <cstdint>
#include <memory>

#include <gtest/gtest.h>

namespace rjk_test {
namespace {

struct Named {
    auto name() const -> std::string_view;
};

struct Alpha {
    auto name() const -> std::string_view { return "alpha"; }
};

struct Beta {
    auto name() const -> std::string_view { return "beta"; }
};

// Larger than the default SBO buffer, forces heap allocation.
struct Heavy {
    std::array<std::byte, 64> padding{};
    auto name() const -> std::string_view { return "heavy"; }
};

struct OtherHeavy {
    std::array<std::byte, 64> padding{};
    auto name() const -> std::string_view { return "other-heavy"; }
};

using NamedDuck = rjk::duck<Named, rjk::copyable>;

TEST(DuckCopy, CopyConstructPreservesValue) {
    NamedDuck x{Alpha{}};
    NamedDuck y{x};
    EXPECT_EQ(y.name(), "alpha");
}

TEST(DuckCopy, CopyConstructIsIndependentOfSource) {
    NamedDuck x{Alpha{}};
    NamedDuck y{x};
    x = Beta{};
    EXPECT_EQ(x.name(), "beta");
    EXPECT_EQ(y.name(), "alpha");
}

TEST(DuckCopy, CopyConstructHeapAllocatedType) {
    NamedDuck x{Heavy{}};
    NamedDuck y{x};
    EXPECT_EQ(y.name(), "heavy");
}

TEST(DuckCopy, CopyAssignPreservesValue) {
    NamedDuck x{Alpha{}};
    NamedDuck y{Beta{}};
    y = x;
    EXPECT_EQ(y.name(), "alpha");
}

TEST(DuckCopy, CopyAssignIsIndependentOfSource) {
    NamedDuck x{Alpha{}};
    NamedDuck y{Beta{}};
    y = x;
    x = Beta{};
    EXPECT_EQ(y.name(), "alpha");
}

TEST(DuckCopy, CopyAssignToSelf) {
    NamedDuck x{Alpha{}};
    x = static_cast<const NamedDuck&>(x);
    EXPECT_EQ(x.name(), "alpha");
}

TEST(DuckCopy, CopyAssignHeapAllocatedType) {
    NamedDuck x{Heavy{}};
    NamedDuck y{Alpha{}};
    y = x;
    EXPECT_EQ(y.name(), "heavy");
}

// Whether d's object lies within d's own bytes.
template <typename T>
bool stored_inside(const NamedDuck& d) {
    const auto obj = reinterpret_cast<std::uintptr_t>(rjk::get_if<T>(&d));
    const auto begin = reinterpret_cast<std::uintptr_t>(&d);
    return obj >= begin && obj < begin + sizeof(d);
}

TEST(DuckStorage, SmallObjectsStayInsideAndLargeOnesOutside) {
    NamedDuck small{Alpha{}};
    NamedDuck large{Heavy{}};
    EXPECT_TRUE(stored_inside<Alpha>(small));
    EXPECT_FALSE(stored_inside<Heavy>(large));

    NamedDuck small_copy{small};
    NamedDuck large_copy{large};
    EXPECT_TRUE(stored_inside<Alpha>(small_copy));
    EXPECT_FALSE(stored_inside<Heavy>(large_copy));

    NamedDuck small_moved{std::move(small_copy)};
    NamedDuck large_moved{std::move(large_copy)};
    EXPECT_TRUE(stored_inside<Alpha>(small_moved));
    EXPECT_FALSE(stored_inside<Heavy>(large_moved));

    small_copy = std::move(large_moved);
    EXPECT_FALSE(stored_inside<Heavy>(small_copy));
    large_copy = std::move(small_moved);
    EXPECT_TRUE(stored_inside<Alpha>(large_copy));

    using std::swap;
    swap(small, large);
    EXPECT_FALSE(stored_inside<Heavy>(small));
    EXPECT_TRUE(stored_inside<Alpha>(large));
    EXPECT_EQ(small.name(), "heavy");
    EXPECT_EQ(large.name(), "alpha");

    rjk::emplace<Beta>(small);
    EXPECT_TRUE(stored_inside<Beta>(small));
    EXPECT_EQ(small.name(), "beta");
}

struct ThreePointers {
    std::array<void*, 3> data{};
    auto name() const -> std::string_view { return "three"; }
};

struct alignas(16) OverAligned {
    auto name() const -> std::string_view { return "aligned"; }
};

TEST(DuckStorage, DefaultBufferHoldsThreePointers) {
    static_assert(sizeof(NamedDuck) == 4 * sizeof(void*));

    NamedDuck three{ThreePointers{}};
    NamedDuck aligned{OverAligned{}};
    EXPECT_TRUE(stored_inside<ThreePointers>(three));
    EXPECT_FALSE(stored_inside<OverAligned>(aligned));
}

TEST(DuckMove, MoveConstructSBO) {
    NamedDuck x{Alpha{}};
    NamedDuck y{std::move(x)};
    EXPECT_EQ(y.name(), "alpha");
    EXPECT_TRUE(rjk::is_valueless(x));
}

TEST(DuckMove, MoveConstructHeap) {
    NamedDuck x{Heavy{}};
    NamedDuck y{std::move(x)};
    EXPECT_EQ(y.name(), "heavy");
    EXPECT_TRUE(rjk::is_valueless(x));
}

TEST(DuckMove, MoveAssignSBO) {
    NamedDuck x{Alpha{}};
    NamedDuck y{Beta{}};
    y = std::move(x);
    EXPECT_EQ(y.name(), "alpha");
    EXPECT_TRUE(rjk::is_valueless(x));
}

TEST(DuckMove, MoveAssignHeap) {
    NamedDuck x{Heavy{}};
    NamedDuck y{Beta{}};
    y = std::move(x);
    EXPECT_EQ(y.name(), "heavy");
    EXPECT_TRUE(rjk::is_valueless(x));
}

TEST(DuckMove, MoveAssignToSelfLeavesAValidState) {
    NamedDuck x{Alpha{}};
    NamedDuck& alias = x;

    x = std::move(alias);

    EXPECT_FALSE(rjk::is_valueless(x));
    x = Beta{};
    EXPECT_EQ(x.name(), "beta");
}

TEST(DuckMove, MoveOnlyUnderlyingType) {
    struct MoveOnly {
        std::unique_ptr<std::string> value;

        explicit MoveOnly(int v) : value(std::make_unique<std::string>(std::to_string(v))) {}

        MoveOnly(const MoveOnly&) = delete;
        MoveOnly& operator=(const MoveOnly&) = delete;
        MoveOnly(MoveOnly&&) = default;
        MoveOnly& operator=(MoveOnly&&) = default;

        auto name() const -> std::string_view { return *value; }
    };

    // Deliberately not rjk::copyable: MoveOnly cannot satisfy it.
    rjk::duck<Named> x{MoveOnly{7}};
    EXPECT_EQ(x.name(), "7");

    auto y = std::move(x);
    EXPECT_EQ(y.name(), "7");
    EXPECT_TRUE(rjk::is_valueless(x));
}

TEST(DuckSwap, SwapSBOWithSBO) {
    NamedDuck x{Alpha{}};
    NamedDuck y{Beta{}};
    rjk::swap(x, y);
    EXPECT_EQ(x.name(), "beta");
    EXPECT_EQ(y.name(), "alpha");
}

TEST(DuckSwap, SwapHeapWithHeap) {
    NamedDuck x{Heavy{}};
    NamedDuck y{OtherHeavy{}};
    rjk::swap(x, y);
    EXPECT_EQ(x.name(), "other-heavy");
    EXPECT_EQ(y.name(), "heavy");
}

TEST(DuckSwap, SwapSBOWithHeap) {
    NamedDuck x{Alpha{}};
    NamedDuck y{Heavy{}};
    rjk::swap(x, y);
    EXPECT_EQ(x.name(), "heavy");
    EXPECT_EQ(y.name(), "alpha");
}

TEST(DuckSwap, SwapHeapWithSBO) {
    NamedDuck x{Heavy{}};
    NamedDuck y{Alpha{}};
    rjk::swap(x, y);
    EXPECT_EQ(x.name(), "alpha");
    EXPECT_EQ(y.name(), "heavy");
}


} // namespace
}
