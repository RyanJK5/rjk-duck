#include "rjk/duck.hpp"
#include "allocators/fixtures.hpp"

#include <map>

#include <gtest/gtest.h>

namespace rjk_test {

// ============================================================
// Instance-counting types
// ============================================================

struct Tracked {
    auto tag() const -> int;
};

// Small enough to live in the SBO buffer.
struct Small {
    inline static thread_local int instance_count = 0;

    Small() { ++instance_count; }
    Small(const Small&) { ++instance_count; }
    Small(Small&&) noexcept { ++instance_count; }
    ~Small() { --instance_count; }
    Small& operator=(const Small&) = default;
    Small& operator=(Small&&) = default;

    auto tag() const -> int { return 1; }
};

// Larger than the default SBO buffer, forces heap allocation.
struct Big {
    inline static thread_local int instance_count = 0;

    std::array<std::byte, 64> padding{};

    Big() { ++instance_count; }
    Big(const Big&) { ++instance_count; }
    Big(Big&&) noexcept { ++instance_count; }
    ~Big() { --instance_count; }
    Big& operator=(const Big&) = default;
    Big& operator=(Big&&) = default;

    auto tag() const -> int { return 2; }
};

// Untracked type used purely as a reassignment target.
struct Other {
    auto tag() const -> int { return 0; }
};

using TrackedDuck = rjk::duck<Tracked, rjk::copyable>;

TEST(DuckLifetime, DestructorRunsWhenDuckGoesOutOfScopeSBO) {
    Small::instance_count = 0;
    {
        TrackedDuck d{Small{}};
        EXPECT_EQ(Small::instance_count, 1);
    }
    EXPECT_EQ(Small::instance_count, 0);
}

TEST(DuckLifetime, DestructorRunsWhenDuckGoesOutOfScopeHeap) {
    Big::instance_count = 0;
    {
        TrackedDuck d{Big{}};
        EXPECT_EQ(Big::instance_count, 1);
    }
    EXPECT_EQ(Big::instance_count, 0);
}

TEST(DuckLifetime, ReassignmentDestroysThePreviousValueSBO) {
    Small::instance_count = 0;
    TrackedDuck d{Small{}};
    EXPECT_EQ(Small::instance_count, 1);
    d = Other{};
    EXPECT_EQ(Small::instance_count, 0);
}

TEST(DuckLifetime, ReassignmentDestroysThePreviousValueHeap) {
    Big::instance_count = 0;
    TrackedDuck d{Big{}};
    EXPECT_EQ(Big::instance_count, 1);
    d = Other{};
    EXPECT_EQ(Big::instance_count, 0);
}

TEST(DuckLifetime, EmplaceDestroysThePreviousValue) {
    Small::instance_count = 0;
    TrackedDuck d{Small{}};
    EXPECT_EQ(Small::instance_count, 1);
    rjk::emplace<Other>(d);
    EXPECT_EQ(Small::instance_count, 0);
}

TEST(DuckLifetime, MoveConstructionTransfersOwnershipSBO) {
    Small::instance_count = 0;
    TrackedDuck x{Small{}};
    EXPECT_EQ(Small::instance_count, 1);
    TrackedDuck y{std::move(x)};
    EXPECT_EQ(Small::instance_count, 1);
}

TEST(DuckLifetime, MoveConstructionTransfersOwnershipHeap) {
    Big::instance_count = 0;
    TrackedDuck x{Big{}};
    EXPECT_EQ(Big::instance_count, 1);
    TrackedDuck y{std::move(x)};
    EXPECT_EQ(Big::instance_count, 1);
}

TEST(DuckLifetime, DestroyingAMovedFromDuckDoesNotDoubleDestroy) {
    Small::instance_count = 0;
    TrackedDuck x{Small{}};
    {
        TrackedDuck y{std::move(x)};
        EXPECT_EQ(Small::instance_count, 1);
    }
    // y's destructor already ran above; x going out of scope below must not
    // decrement the count a second time, since ownership was transferred.
    EXPECT_EQ(Small::instance_count, 0);
}

TEST(DuckLifetime, DestroyingACopyDoesNotAffectTheOriginal) {
    Small::instance_count = 0;
    TrackedDuck x{Small{}};
    {
        TrackedDuck y{x};
        EXPECT_EQ(Small::instance_count, 2);
    }
    EXPECT_EQ(Small::instance_count, 1);
}

TEST(DuckLifetime, SwapDoesNotChangeTotalInstanceCount) {
    Small::instance_count = 0;
    Big::instance_count = 0;
    TrackedDuck x{Small{}};
    TrackedDuck y{Big{}};
    std::swap(x, y);
    EXPECT_EQ(Small::instance_count, 1);
    EXPECT_EQ(Big::instance_count, 1);
}

TEST(DuckLifetime, SelfMoveAssignmentDoesNotLeak) {
    Small::instance_count = 0;
    TrackedDuck x{Small{}};
    TrackedDuck& alias = x;

    x = std::move(alias);
    EXPECT_EQ(Small::instance_count, 1);
}

// ============================================================
// Storage lifetime regression tests
// ============================================================
//
// These types record every live object's address in a ledger, so building an
// object on top of a live one, destroying an object twice, or running the
// wrong type's destructor is caught even when the per-type counts balance.

namespace {

struct Identified {
    auto id() const -> int;
};

std::map<const void*, char>& live_objects() {
    static std::map<const void*, char> objects;
    return objects;
}

int lifetime_errors = 0;

void on_construct(const void* obj, char tag) {
    auto [itr, inserted] = live_objects().emplace(obj, tag);
    if (!inserted) {
        // Constructed on top of an object that was never destroyed.
        ++lifetime_errors;
        itr->second = tag;
    }
}

void on_destroy(const void* obj, char tag) {
    const auto itr = live_objects().find(obj);
    if (itr == live_objects().end() || itr->second != tag) {
        // Double destroy, or a destructor run on an object of another type.
        ++lifetime_errors;
        return;
    }
    live_objects().erase(itr);
}

template <char Tag, std::size_t Padding = 0>
struct Counted {
    inline static int constructed = 0;
    inline static int destroyed = 0;

    static int live() { return constructed - destroyed; }
    static void reset() { constructed = destroyed = 0; }

    int value;
    std::array<std::byte, Padding> padding{};

    explicit Counted(int v) : value(v) { record_construct(); }
    Counted(const Counted& other) : value(other.value) { record_construct(); }
    Counted(Counted&& other) noexcept : value(other.value) { record_construct(); }
    ~Counted() {
        on_destroy(this, Tag);
        ++destroyed;
    }
    Counted& operator=(const Counted&) = default;
    Counted& operator=(Counted&&) = default;

    auto id() const -> int { return value; }

private:
    void record_construct() {
        on_construct(this, Tag);
        ++constructed;
    }
};

using InlineA = Counted<'A'>;
using InlineB = Counted<'B'>;
using HeapC = Counted<'C', 64>;

struct CopyFailure {};

// Inline type whose copy constructor writes into its destination before
// throwing, so a copy built over a live object visibly corrupts it.
struct ThrowingCopy {
    inline static bool throw_on_copy = false;

    // volatile so the store before the throw can't be optimised away
    volatile int value;

    explicit ThrowingCopy(int v) : value(v) { on_construct(this, 'T'); }
    ThrowingCopy(const ThrowingCopy& other) : value(other.value) {
        if (throw_on_copy) {
            throw CopyFailure{};
        }
        on_construct(this, 'T');
    }
    ThrowingCopy(ThrowingCopy&& other) noexcept : value(other.value) { on_construct(this, 'T'); }
    ~ThrowingCopy() { on_destroy(this, 'T'); }

    auto id() const -> int { return value; }
};

using IdDuck = rjk::duck<Identified, rjk::copyable>;

template <rjk::is_perf_option Perf>
using IdAllocDuck = rjk::duck<Identified, rjk::copyable, Perf>;

class StorageLifetime : public ::testing::Test {
protected:
    void SetUp() override {
        live_objects().clear();
        lifetime_errors = 0;
        InlineA::reset();
        InlineB::reset();
        HeapC::reset();
        ThrowingCopy::throw_on_copy = false;
    }

    void TearDown() override {
        EXPECT_EQ(lifetime_errors, 0);
        EXPECT_TRUE(live_objects().empty());
    }
};

// ---- copy assignment ----

TEST_F(StorageLifetime, CopyAssignInlineOverInline) {
    {
        IdDuck a{std::in_place_type<InlineA>, 1};
        const IdDuck b{std::in_place_type<InlineB>, 2};

        a = b;

        EXPECT_EQ(lifetime_errors, 0);
        EXPECT_EQ(InlineA::destroyed, 1);
        EXPECT_EQ(InlineA::live(), 0);
        EXPECT_EQ(InlineB::live(), 2);
        EXPECT_NE(rjk::get_if<InlineB>(&a), nullptr);
        EXPECT_EQ(a.id(), 2);
    }
    EXPECT_EQ(InlineA::destroyed, 1);
    EXPECT_EQ(InlineB::live(), 0);
}

TEST_F(StorageLifetime, CopyAssignHeapOverInline) {
    {
        IdDuck a{std::in_place_type<InlineA>, 1};
        const IdDuck b{std::in_place_type<HeapC>, 3};

        a = b;

        EXPECT_EQ(InlineA::destroyed, 1);
        EXPECT_EQ(HeapC::live(), 2);
        EXPECT_NE(rjk::get_if<HeapC>(&a), nullptr);
        EXPECT_EQ(a.id(), 3);
    }
    EXPECT_EQ(InlineA::live(), 0);
    EXPECT_EQ(HeapC::live(), 0);
}

TEST_F(StorageLifetime, CopyAssignInlineOverHeap) {
    {
        IdDuck a{std::in_place_type<HeapC>, 3};
        const IdDuck b{std::in_place_type<InlineA>, 1};

        a = b;

        EXPECT_EQ(HeapC::destroyed, 1);
        EXPECT_EQ(InlineA::live(), 2);
        EXPECT_NE(rjk::get_if<InlineA>(&a), nullptr);
        EXPECT_EQ(a.id(), 1);
    }
    EXPECT_EQ(InlineA::live(), 0);
    EXPECT_EQ(HeapC::live(), 0);
}

TEST_F(StorageLifetime, CopySelfAssign) {
    {
        IdDuck a{std::in_place_type<InlineA>, 1};
        const IdDuck& alias = a;

        a = alias;

        EXPECT_EQ(InlineA::constructed, 1);
        EXPECT_EQ(InlineA::live(), 1);
        EXPECT_EQ(a.id(), 1);
    }
    EXPECT_EQ(InlineA::destroyed, 1);
}

TEST_F(StorageLifetime, ThrowingCopyAssignLeavesTargetUnchanged) {
    {
        IdDuck a{std::in_place_type<InlineA>, 1};
        const IdDuck b{std::in_place_type<ThrowingCopy>, 7};

        ThrowingCopy::throw_on_copy = true;
        EXPECT_THROW(a = b, CopyFailure);
        ThrowingCopy::throw_on_copy = false;

        EXPECT_EQ(InlineA::live(), 1);
        EXPECT_NE(rjk::get_if<InlineA>(&a), nullptr);
        EXPECT_EQ(a.id(), 1);
        EXPECT_EQ(b.id(), 7);
    }
    EXPECT_EQ(InlineA::destroyed, 1);
}

TEST_F(StorageLifetime, PoccaCopyAssignInlineOverInline) {
    AllocCounter counter_a{};
    AllocCounter counter_b{};
    {
        IdAllocDuck<Pocca> a{std::allocator_arg, PoccaAlloc{counter_a}, std::in_place_type<InlineA>, 1};
        const IdAllocDuck<Pocca> b{std::allocator_arg, PoccaAlloc{counter_b}, std::in_place_type<InlineB>, 2};

        a = b;

        EXPECT_EQ(InlineA::destroyed, 1);
        EXPECT_EQ(InlineB::live(), 2);
        EXPECT_EQ(a.id(), 2);
        EXPECT_TRUE(rjk::get_allocator(a) == rjk::get_allocator(b));
    }
    EXPECT_EQ(InlineB::live(), 0);
}

TEST_F(StorageLifetime, PoccaCopyAssignHeapUsesSourceAllocator) {
    AllocCounter counter_a{};
    AllocCounter counter_b{};
    {
        IdAllocDuck<Pocca> a{std::allocator_arg, PoccaAlloc{counter_a}, std::in_place_type<InlineA>, 1};
        const IdAllocDuck<Pocca> b{std::allocator_arg, PoccaAlloc{counter_b}, std::in_place_type<HeapC>, 3};

        a = b;

        EXPECT_EQ(counter_a.allocs, 0);
        EXPECT_EQ(counter_b.allocs, 2);
        EXPECT_EQ(InlineA::destroyed, 1);
        EXPECT_EQ(a.id(), 3);
    }
    EXPECT_EQ(counter_b.outstanding(), 0);
    EXPECT_EQ(HeapC::live(), 0);
}

}

}
