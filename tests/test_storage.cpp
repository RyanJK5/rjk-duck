#include "rjk/duck.hpp"
#include "test_fixtures.hpp"

#include <gtest/gtest.h>

namespace rjk_test {

using BlankStorage = rjk::detail::storage<
    rjk::detail::vtable_generator<>
>;
using CopyableStorage = rjk::detail::storage<
    rjk::detail::vtable_generator<rjk::copyable>
>;

struct [[=rjk::perf_options]] HeapOnly {
    std::size_t sbo_size = 0;
    std::size_t sbo_alignment = 32;

    struct inlined_functions {};
};


using HeapOnlyStorage = rjk::detail::storage<
    rjk::detail::vtable_generator<HeapOnly>
>;
static_assert(!HeapOnlyStorage::template fits_sbo<int>);

TEST(StorageInternals, HasTypeSBO) {
    BlankStorage s{std::in_place_type<A>};
    EXPECT_TRUE(s.has_type<A>());
    EXPECT_FALSE(s.has_type<B>());
}

TEST(StorageInternals, HasTypeHeap) {
    BlankStorage s{std::in_place_type<Big>};
    EXPECT_TRUE(s.has_type<Big>());
    EXPECT_FALSE(s.has_type<A>());
}

TEST(StorageInternals, EmplaceOverwrites) {
    BlankStorage s{std::in_place_type<A>};
    EXPECT_TRUE(s.has_type<A>());
    s.emplace<B>();
    EXPECT_TRUE(s.has_type<B>());
}

TEST(StorageInternals, MoveSBO) {
    BlankStorage s{std::in_place_type<A>};
    BlankStorage t{std::move(s)};
    EXPECT_TRUE(t.has_value());
    EXPECT_FALSE(s.has_value());
}

TEST(StorageInternals, MoveHeap) {
    BlankStorage s{std::in_place_type<Big>};
    BlankStorage t{std::move(s)};
    EXPECT_TRUE(t.has_value());
    EXPECT_FALSE(s.has_value());
}

TEST(StorageInternals, CopySBO) {
    CopyableStorage s{std::in_place_type<A>};
    CopyableStorage t{s};
    EXPECT_TRUE(t.has_value());
    EXPECT_TRUE(s.has_value());
}

TEST(StorageInternals, CopyHeap) {
    CopyableStorage s{std::in_place_type<Big>};
    CopyableStorage t{s};
    EXPECT_TRUE(t.has_value());
    EXPECT_TRUE(s.has_value());
}

TEST(StorageInternals, CopyAssignSBO) {
    CopyableStorage s{std::in_place_type<A>};
    CopyableStorage t{std::in_place_type<B>};
    t = s;
    EXPECT_TRUE(t.has_type<A>());
}

TEST(StorageInternals, CopyAssignHeap) {
    CopyableStorage s{std::in_place_type<Big>};
    CopyableStorage t{std::in_place_type<Big>};
    t = s;
    EXPECT_TRUE(t.has_type<Big>());
}

TEST(StorageInternals, MoveAssignSBO) {
    BlankStorage s{std::in_place_type<A>};
    BlankStorage t{std::in_place_type<B>};
    t = std::move(s);
    EXPECT_TRUE(t.has_type<A>());
    EXPECT_FALSE(s.has_value());
}

TEST(StorageInternals, MoveAssignHeap) {
    BlankStorage s{std::in_place_type<Big>};
    BlankStorage t{std::in_place_type<Big>};
    t = std::move(s);
    EXPECT_TRUE(t.has_type<Big>());
    EXPECT_FALSE(s.has_value());
}

TEST(StorageInternals, ResetClearsValue) {
    BlankStorage s{std::in_place_type<A>};
    s.reset();
    EXPECT_FALSE(s.has_value());
}

TEST(StorageInternals, GetReturnsNonNull) {
    BlankStorage s{std::in_place_type<A>};
    EXPECT_NE(s.get(), nullptr);
    const BlankStorage cs{std::in_place_type<A>};
    EXPECT_NE(cs.get(), nullptr);
}
}
