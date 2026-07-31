#include "fixtures.hpp"

#include <gtest/gtest.h>

namespace rjk_test {

template <typename T>
struct SocccTestAlloc {
    using value_type = T;
    using propagate_on_container_copy_assignment = std::false_type;
    using propagate_on_container_move_assignment = std::false_type;
    using propagate_on_container_swap = std::false_type;

    template <typename U>
    struct rebind {
        using other = SocccTestAlloc<U>;
    };

    explicit SocccTestAlloc(AllocCounter& counter) noexcept
        : m_counter(&counter)
    { }

    template <typename U>
    SocccTestAlloc(const SocccTestAlloc<U>& other) noexcept
        : m_counter(other.m_counter)
    { }

    SocccTestAlloc(const SocccTestAlloc& other) noexcept
        : m_counter(other.m_counter)
    { }

    static AllocCounter& default_counter() {
        static AllocCounter instance{};
        return instance;
    }

    SocccTestAlloc select_on_container_copy_construction() const noexcept {
        return SocccTestAlloc{default_counter()};
    }

    T* allocate(std::size_t n) {
        m_counter->allocs++;
        return static_cast<T*>(::operator new(n * sizeof(T)));
    }

    void deallocate(T* p, std::size_t) noexcept {
        m_counter->deallocs++;
        ::operator delete(p);
    }

    template <typename U>
    bool operator==(const SocccTestAlloc<U>& other) const noexcept {
        return m_counter == other.m_counter;
    }

    AllocCounter* m_counter;
};

using SocccAlloc = SocccTestAlloc<std::byte>;
struct [[=rjk::perf_options]] Soccc {
    using allocator = SocccAlloc;
};

TEST(DuckAllocatorSoccc, PlainCopyUsesSelectOnContainerCopyConstruction) {
    AllocCounter counterSrc{};
    SocccAlloc allocSrc{counterSrc};

    AllocDuck<Soccc> original{std::allocator_arg, allocSrc, BigWidget{301}};
    ASSERT_EQ(counterSrc.allocs, 1);

    AllocDuck<Soccc> copy{original}; // no allocator supplied -> must go through SOCCC

    EXPECT_EQ(copy.to_string(), "BigWidget(301)");

    EXPECT_TRUE(rjk::get_allocator(copy) == SocccAlloc{SocccAlloc::default_counter()});
    EXPECT_FALSE(rjk::get_allocator(copy) == allocSrc);
    EXPECT_EQ(counterSrc.allocs, 1); // source's counter must not see the copy's allocation
}

TEST(DuckAllocatorSoccc, AllocatorExtendedCopyBypassesSoccc) {
    AllocCounter counterSrc{};
    AllocCounter counterExplicit{};
    SocccAlloc allocSrc{counterSrc};
    SocccAlloc allocExplicit{counterExplicit};

    AllocDuck<Soccc> original{std::allocator_arg, allocSrc, BigWidget{302}};
    ASSERT_EQ(counterSrc.allocs, 1);

    AllocDuck<Soccc> copy{std::allocator_arg, allocExplicit, original};

    EXPECT_EQ(copy.to_string(), "BigWidget(302)");
    EXPECT_TRUE(rjk::get_allocator(copy) == allocExplicit);
    EXPECT_EQ(counterExplicit.allocs, 1);
    EXPECT_EQ(counterSrc.allocs, 1);
}

}  // namespace rjk_test