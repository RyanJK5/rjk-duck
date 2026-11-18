#include "rjk/duck.hpp"
#include "fixtures.hpp"

#include <gtest/gtest.h>
#include <memory>
#include <new>
#include <type_traits>
#include <vector>

namespace rjk_test {
namespace {

// An allocator whose construct and destroy can be observed.
template <typename T>
struct ConstructCountingAlloc {
    using value_type = T;

    ConstructCountingAlloc(int& constructs, int& destroys) noexcept
        : constructs{&constructs}, destroys{&destroys}
    { }

    template <typename U>
    ConstructCountingAlloc(const ConstructCountingAlloc<U>& other) noexcept
        : constructs{other.constructs}, destroys{other.destroys}
    { }

    T* allocate(std::size_t n) { return std::allocator<T>{}.allocate(n); }
    void deallocate(T* p, std::size_t n) noexcept { std::allocator<T>{}.deallocate(p, n); }

    template <typename U, typename... Args>
    void construct(U* p, Args&&... args) noexcept(std::is_nothrow_constructible_v<U, Args...>) {
        ++*constructs;
        std::construct_at(p, std::forward<Args>(args)...);
    }

    template <typename U>
    void destroy(U* p) noexcept {
        ++*destroys;
        std::destroy_at(p);
    }

    template <typename U>
    bool operator==(const ConstructCountingAlloc<U>& other) const noexcept {
        return constructs == other.constructs;
    }

    int* constructs;
    int* destroys;
};

struct [[=rjk::perf_options]] ConstructCounting {
    using allocator = ConstructCountingAlloc<std::byte>;
};

// An allocator whose construct names the constructor in its exception
// specification, as a standard library's might.
template <typename T>
struct ExactNoexceptAlloc {
    using value_type = T;

    ExactNoexceptAlloc() = default;

    template <typename U>
    ExactNoexceptAlloc(const ExactNoexceptAlloc<U>&) noexcept {}

    T* allocate(std::size_t n) { return std::allocator<T>{}.allocate(n); }
    void deallocate(T* p, std::size_t n) noexcept { std::allocator<T>{}.deallocate(p, n); }

    template <typename U, typename... Args>
    void construct(U* p, Args&&... args)
        noexcept(noexcept(::new (static_cast<void*>(p)) U(std::forward<Args>(args)...))) {
        ::new (static_cast<void*>(p)) U(std::forward<Args>(args)...);
    }

    friend bool operator==(const ExactNoexceptAlloc&, const ExactNoexceptAlloc&) = default;
};

struct [[=rjk::perf_options]] ExactNoexcept {
    using allocator = ExactNoexceptAlloc<std::byte>;
};

struct Gettable {
    auto get() const -> int;
};

struct SmallGettable {
    int get() const { return 1; }
};

struct PinnedGettable {
    PinnedGettable() = default;
    PinnedGettable(PinnedGettable&&) = delete;
    int get() const { return 2; }
};

// Asking whether a type can be buffered doesn't ask the allocator to move a
// type that can't be moved, which would be a hard error here.
using ExactDuck = rjk::duck<Gettable, ExactNoexcept>;
static_assert(std::is_nothrow_constructible_v<ExactDuck, std::in_place_type_t<SmallGettable>>);
static_assert(!std::is_nothrow_constructible_v<ExactDuck, std::in_place_type_t<PinnedGettable>>);

TEST(BasicAllocator, BufferedObjectsAreConstructedThroughTheAllocator) {
    int constructs{};
    int destroys{};
    ConstructCountingAlloc<std::byte> alloc{constructs, destroys};

    {
        rjk::duck<Stringify, ConstructCounting> a{std::allocator_arg, alloc, Widget{1}};
        EXPECT_EQ(constructs, 1);

        // Relocating into b constructs there and destroys a's.
        rjk::duck<Stringify, ConstructCounting> b{std::move(a)};
        EXPECT_EQ(constructs, 2);
        EXPECT_EQ(destroys, 1);
        EXPECT_EQ(b.to_string(), "Widget(1)");
    }

    EXPECT_EQ(destroys, 2);
}

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

TEST(BasicAllocator, InPlaceConstructor) {
    AllocCounter counter{};
    AlwaysEqualAlloc alloc{counter};

    AllocDuck<AlwaysEqual> d{std::allocator_arg, alloc, std::in_place_type<BigWidget>, 201};

    EXPECT_EQ(d.to_string(), "BigWidget(201)");
    EXPECT_EQ(counter.allocs, 1);
    EXPECT_EQ(counter.deallocs, 0);
}

TEST(BasicAllocator, InPlaceInitList) {
    struct BigListWidget {
        std::array<char, 64> padding{};
        std::vector<int> values;

        BigListWidget(std::initializer_list<int> il, int) : values(il) {}
        auto to_string() const -> std::string {
            return "BigListWidget(" + std::to_string(values.size()) + ")";
        }
    };
    
    AllocCounter counter{};
    AlwaysEqualAlloc alloc{counter};

    AllocDuck<AlwaysEqual> d{std::allocator_arg, alloc, std::in_place_type<BigListWidget>, {1, 2, 3}, 0};

    EXPECT_EQ(d.to_string(), "BigListWidget(3)");
    EXPECT_EQ(counter.allocs, 1);
    EXPECT_EQ(counter.deallocs, 0);
}

TEST(DuckAllocator, GetAllocator) {
    AllocCounter counter{};
    AlwaysEqualAlloc alloc{counter};

    AllocDuck<AlwaysEqual> d{std::allocator_arg, alloc, BigWidget{201}};

    EXPECT_EQ(rjk::get_allocator(d), alloc);
}

TEST(DuckAllocator, GetAllocatorPocma) {
    AllocCounter counterA{};
    AllocCounter counterB{};
    PocmaAlloc allocA{counterA};
    PocmaAlloc allocB{counterB};

    AllocDuck<Pocma> a{std::allocator_arg, allocA, BigWidget{202}};
    AllocDuck<Pocma> b{std::allocator_arg, allocB, BigWidget{203}};

    b = std::move(a);

    EXPECT_EQ(rjk::get_allocator(b), allocA);
    EXPECT_NE(rjk::get_allocator(b), allocB);
}


} // namespace
}
