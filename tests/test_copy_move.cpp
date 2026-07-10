#include "rjk/duck.hpp"

#include <memory>

#include <gtest/gtest.h>

namespace rjk_test {

struct [[=rjk::trait]] Named {
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

TEST(DuckMove, MoveConstructSBO) {
    NamedDuck x{Alpha{}};
    NamedDuck y{std::move(x)};
    EXPECT_EQ(y.name(), "alpha");
}

TEST(DuckMove, MoveConstructHeap) {
    NamedDuck x{Heavy{}};
    NamedDuck y{std::move(x)};
    EXPECT_EQ(y.name(), "heavy");
}

TEST(DuckMove, MoveAssignSBO) {
    NamedDuck x{Alpha{}};
    NamedDuck y{Beta{}};
    y = std::move(x);
    EXPECT_EQ(y.name(), "alpha");
}

TEST(DuckMove, MoveAssignHeap) {
    NamedDuck x{Heavy{}};
    NamedDuck y{Beta{}};
    y = std::move(x);
    EXPECT_EQ(y.name(), "heavy");
}

TEST(DuckMove, MoveAssignToSelfLeavesAValidState) {
    NamedDuck x{Alpha{}};
    NamedDuck& alias = x;

    x = std::move(alias);
    // Post self-move state is valid but unspecified; the important part is
    // that the duck can still be safely destroyed and reassigned afterward.
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
}

TEST(DuckSwap, SwapSBOWithSBO) {
    NamedDuck x{Alpha{}};
    NamedDuck y{Beta{}};
    std::swap(x, y);
    EXPECT_EQ(x.name(), "beta");
    EXPECT_EQ(y.name(), "alpha");
}

TEST(DuckSwap, SwapHeapWithHeap) {
    NamedDuck x{Heavy{}};
    NamedDuck y{OtherHeavy{}};
    std::swap(x, y);
    EXPECT_EQ(x.name(), "other-heavy");
    EXPECT_EQ(y.name(), "heavy");
}

TEST(DuckSwap, SwapSBOWithHeap) {
    NamedDuck x{Alpha{}};
    NamedDuck y{Heavy{}};
    std::swap(x, y);
    EXPECT_EQ(x.name(), "heavy");
    EXPECT_EQ(y.name(), "alpha");
}

TEST(DuckSwap, SwapHeapWithSBO) {
    NamedDuck x{Heavy{}};
    NamedDuck y{Alpha{}};
    std::swap(x, y);
    EXPECT_EQ(x.name(), "alpha");
    EXPECT_EQ(y.name(), "heavy");
}

}