#include "duck.hpp"
#include "test_fixtures.hpp"

#include <gtest/gtest.h>

namespace rjk_test {
TEST(StorageInternals, SBOFitsSmallType) {
    EXPECT_TRUE(rjk::detail::fits_sbo<A>);
}

TEST(StorageInternals, SBODoesNotFitBigType) {
    EXPECT_FALSE(rjk::detail::fits_sbo<Big>);
}

TEST(StorageInternals, HasTypeSBO) {
    rjk::detail::storage s{std::in_place_type<A>};
    EXPECT_TRUE(s.has_type<A>());
    EXPECT_FALSE(s.has_type<B>());
}

TEST(StorageInternals, HasTypeHeap) {
    rjk::detail::storage s{std::in_place_type<Big>};
    EXPECT_TRUE(s.has_type<Big>());
    EXPECT_FALSE(s.has_type<A>());
}

TEST(StorageInternals, DefaultHasNoValue) {
    rjk::detail::storage s{};
    EXPECT_FALSE(s.has_value());
}

TEST(StorageInternals, EmplaceOverwrites) {
    rjk::detail::storage s{std::in_place_type<A>};
    EXPECT_TRUE(s.has_type<A>());
    s.emplace<B>();
    EXPECT_TRUE(s.has_type<B>());
}

TEST(StorageInternals, MoveSBO) {
    rjk::detail::storage s{std::in_place_type<A>};
    rjk::detail::storage t{std::move(s)};
    EXPECT_TRUE(t.has_value());
    EXPECT_FALSE(s.has_value());
}

TEST(StorageInternals, MoveHeap) {
    rjk::detail::storage s{std::in_place_type<Big>};
    rjk::detail::storage t{std::move(s)};
    EXPECT_TRUE(t.has_value());
    EXPECT_FALSE(s.has_value());
}

TEST(StorageInternals, CopySBO) {
    rjk::detail::storage s{std::in_place_type<A>};
    rjk::detail::storage t{s};
    EXPECT_TRUE(t.has_value());
    EXPECT_TRUE(s.has_value());
}

TEST(StorageInternals, CopyHeap) {
    rjk::detail::storage s{std::in_place_type<Big>};
    rjk::detail::storage t{s};
    EXPECT_TRUE(t.has_value());
    EXPECT_TRUE(s.has_value());
}

TEST(StorageInternals, CopyAssignSBO) {
    rjk::detail::storage s{std::in_place_type<A>};
    rjk::detail::storage t{std::in_place_type<B>};
    t = s;
    EXPECT_TRUE(t.has_type<A>());
}

TEST(StorageInternals, CopyAssignHeap) {
    rjk::detail::storage s{std::in_place_type<Big>};
    rjk::detail::storage t{std::in_place_type<Big>};
    t = s;
    EXPECT_TRUE(t.has_type<Big>());
}

TEST(StorageInternals, MoveAssignSBO) {
    rjk::detail::storage s{std::in_place_type<A>};
    rjk::detail::storage t{std::in_place_type<B>};
    t = std::move(s);
    EXPECT_TRUE(t.has_type<A>());
    EXPECT_FALSE(s.has_value());
}

TEST(StorageInternals, MoveAssignHeap) {
    rjk::detail::storage s{std::in_place_type<Big>};
    rjk::detail::storage t{std::in_place_type<Big>};
    t = std::move(s);
    EXPECT_TRUE(t.has_type<Big>());
    EXPECT_FALSE(s.has_value());
}

TEST(StorageInternals, ResetClearsValue) {
    rjk::detail::storage s{std::in_place_type<A>};
    s.reset();
    EXPECT_FALSE(s.has_value());
}

TEST(StorageInternals, UsingsSBO) {
    rjk::detail::storage s{std::in_place_type<A>};
    EXPECT_TRUE(s.using_sbo());
    rjk::detail::storage t{std::in_place_type<Big>};
    EXPECT_FALSE(t.using_sbo());
}

TEST(StorageInternals, GetReturnsNonNull) {
    rjk::detail::storage s{std::in_place_type<A>};
    EXPECT_NE(s.get(), nullptr);
    const rjk::detail::storage cs{std::in_place_type<A>};
    EXPECT_NE(cs.get(), nullptr);
}
}
