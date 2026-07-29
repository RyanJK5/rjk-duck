#ifndef RJK_DUCK_FIXTURES_HPP
#define RJK_DUCK_FIXTURES_HPP

#include "rjk/duck.hpp"

namespace rjk_test {

struct AllocCounter {
    int allocs{};
    int deallocs{};
    int copies{};
    int moves{};

    int outstanding() const { return allocs - deallocs; }
};

template <typename T, bool Pocca, bool Pocma, bool AlwaysEqual = false>
struct PropagatingTestAlloc {
    using value_type = T;
    using propagate_on_container_copy_assignment = std::bool_constant<Pocca>;
    using propagate_on_container_move_assignment = std::bool_constant<Pocma>;
    using is_always_equal = std::bool_constant<AlwaysEqual>;

    template <typename U>
    struct rebind {
        using other = PropagatingTestAlloc<U, Pocca, Pocma, AlwaysEqual>;
    };

    explicit PropagatingTestAlloc(AllocCounter& counter) noexcept
        : m_counter(&counter)
    { }

    template <typename U>
    PropagatingTestAlloc(const PropagatingTestAlloc<U, Pocca, Pocma, AlwaysEqual>& other) noexcept
        : m_counter(other.m_counter) {
        m_counter->copies++;
    }

    template <typename U>
    PropagatingTestAlloc(PropagatingTestAlloc<U, Pocca, Pocma, AlwaysEqual>&& other) noexcept
        : m_counter(std::exchange(other.m_counter, nullptr)) {
        m_counter->moves++;
    }

    template <typename U>
    PropagatingTestAlloc& operator=(const PropagatingTestAlloc<U, Pocca, Pocma, AlwaysEqual>& other) noexcept {
        if (this != &other) {
            m_counter = other.m_counter;
            m_counter->copies++;
        }
        return *this;
    }

    template <typename U>
    PropagatingTestAlloc& operator=(PropagatingTestAlloc<U, Pocca, Pocma, AlwaysEqual>&& other) noexcept {
        if (this != &other) {
            m_counter = std::exchange(other.m_counter, nullptr);
            m_counter->moves++;
        }
        return *this;
    }

    PropagatingTestAlloc(const PropagatingTestAlloc& other) noexcept
    : m_counter(other.m_counter) {
        m_counter->copies++;
    }

    PropagatingTestAlloc(PropagatingTestAlloc&& other) noexcept
        : m_counter(std::exchange(other.m_counter, nullptr)) {
        m_counter->moves++;
    }

    PropagatingTestAlloc& operator=(const PropagatingTestAlloc& other) noexcept {
        if (this != &other) {
            m_counter = other.m_counter;
            m_counter->copies++;
        }
        return *this;
    }

    PropagatingTestAlloc& operator=(PropagatingTestAlloc&& other) noexcept {
        if (this != &other) {
            m_counter = std::exchange(other.m_counter, nullptr);
            m_counter->moves++;
        }
        return *this;
    }

    T* allocate(std::size_t n) {
        if (m_counter) {
            m_counter->allocs++;
        }
        return static_cast<T*>(::operator new(n * sizeof(T)));
    }

    void deallocate(T* p, std::size_t) noexcept {
        if (m_counter) {
            m_counter->deallocs++;
        }
        ::operator delete(p);
    }

    template <typename U>
    bool operator==(const PropagatingTestAlloc<U, Pocca, Pocma, AlwaysEqual>& other) const noexcept {
        return m_counter == other.m_counter;
    }

private:
    template <typename U, bool C, bool M, bool AE>
    friend struct PropagatingTestAlloc;

    AllocCounter* m_counter;
};

using BothAlloc = PropagatingTestAlloc<std::byte, true, true>;
struct [[=rjk::perf_options]] Both {
    using allocator = BothAlloc;
};

using PoccaAlloc = PropagatingTestAlloc<std::byte, true, false>;
struct [[=rjk::perf_options]] Pocca {
    using allocator = PoccaAlloc;
};

using PocmaAlloc = PropagatingTestAlloc<std::byte, false, true>;
struct [[=rjk::perf_options]] Pocma {
    using allocator = PocmaAlloc;
};

using NoneAlloc = PropagatingTestAlloc<std::byte, false, false>;
struct [[=rjk::perf_options]] None {
    using allocator = NoneAlloc;
};

using AlwaysEqualAlloc = PropagatingTestAlloc<std::byte, false, false, true>;
struct [[=rjk::perf_options]] AlwaysEqual {
    using allocator = AlwaysEqualAlloc;
};

struct Stringify {
    auto to_string() const -> std::string;
};

template <rjk::is_perf_option Perf>
using AllocDuck = rjk::duck<Stringify, rjk::copyable, Perf>;

struct Widget {
    int value{};
    auto to_string() const -> std::string { return "Widget(" + std::to_string(value) + ")"; }
};

struct BigWidget {
    std::array<char, 64> padding{};
    int tag{};

    explicit BigWidget(int t = 0) : tag(t) {}
    auto to_string() const -> std::string { return "BigWidget(" + std::to_string(tag) + ")"; }
};
}

#endif //RJK_DUCK_FIXTURES_HPP
