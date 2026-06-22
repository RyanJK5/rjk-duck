// clang-format off

#ifndef RJK_DUCK_VIEW_HPP
#define RJK_DUCK_VIEW_HPP

#include "ducwk.hpp"
#include "detail/subsumption_utils.hpp"

namespace rjk {

template <is_trait... Traits>
class duck_ptr;

template <is_trait... Traits>
class duck_view
    : public detail::duck_behavior_base<duck_view<Traits...>, Traits...> {
private:
    using duck_base_t = detail::make_duck_base_t<duck_view, Traits...>;

    constexpr static bool all_const = sizeof...(Traits) > 0 && (std::is_const_v<Traits> && ...);

    using underlying_ptr_t = std::conditional_t<all_const, const void*, void*>;

    using util = detail::subsumption_utils<Traits...>;
public:
    template <typename T> requires
        (!detail::is_duck_type(^^T) &&
        duck_base_t::template meets_tags<T>())
    constexpr duck_view(T&& obj) noexcept
        : m_underlying(std::addressof(obj))
        , m_vtable(&duck_base_t::template static_vtable_for<std::decay_t<T>>) {
        static_assert(all_const || !std::is_const_v<std::remove_reference_t<T>>,
            "Cannot bind duck_view with mutable traits to a const object");
    }

    template <typename Duck>
    constexpr duck_view(Duck&& d) noexcept requires (
        !std::same_as<std::decay_t<Duck>, duck_view> &&
        util::total_subsumption(decay(^^Duck))
    )
        : m_underlying(d.get_underlying())
        , m_vtable(d.get_vtable())
    { }

    template <typename Duck> requires (util::total_const_subsumption(decay(^^Duck)))
    constexpr duck_view(Duck&& d) noexcept
        : m_underlying(d.get_underlying())
        , m_vtable(d.get_vtable()->to_const)
    { }

    template <typename Duck> requires (util::single_trait_subsumption(decay(^^Duck)))
    constexpr duck_view(Duck&& d) noexcept
        : m_underlying(d.get_underlying())
        , m_vtable(util::template convert_from<Duck>(d.get_vtable()))
    { }

    template <std::meta::info VtableMember, duck_tag Tag, detail::fn_qualifiers Qualifiers, typename Func>
    friend class duck_base_t::vtable_function;

    template <typename Derived, is_trait... BaseTraits>
    friend class detail::duck_behavior_base;

    template <is_trait... ViewTraits>
    friend class duck_view;

    template <is_trait... DuckTraits>
    friend class duck;

    friend class duck_ptr<Traits...>;
private:
    constexpr duck_view() noexcept : m_underlying(nullptr), m_vtable(nullptr) { }

    template <typename T>
    constexpr bool has_type() const noexcept { return m_vtable == &duck_base_t::template static_vtable_for<T>; }

    constexpr const auto* get_vtable() const noexcept { return m_vtable; }

    constexpr underlying_ptr_t get_underlying() const noexcept { return m_underlying; }
private:
    underlying_ptr_t m_underlying;
    const duck_base_t::vtable* m_vtable;
};

template <is_trait... Traits>
duck_view(duck<Traits...>&) -> duck_view<Traits...>;

template <is_trait... Traits>
duck_view(duck<Traits...>&&) -> duck_view<Traits...>;

template <is_trait... Traits>
duck_view(const duck<Traits...>&) -> duck_view<const Traits...>;

template <typename T, is_trait... Traits> requires
    (!std::same_as<std::decay_t<T>, duck<Traits...>> &&
    !std::same_as<std::decay_t<T>, duck_view<Traits...>>)
duck_view(T&&) -> duck_view<>;

template <is_trait... Traits>
class duck_ptr {
private:
    using duck_base_t = duck_view<Traits...>::duck_base_t;
    using util = detail::subsumption_utils<Traits...>;
public:
    constexpr duck_ptr() noexcept = default;
    constexpr duck_ptr(std::nullopt_t) noexcept {}

    constexpr duck_ptr(duck_view<Traits...> view) noexcept : m_view(view) {}

    template <typename T>
    constexpr duck_ptr(T&& obj) noexcept : m_view(std::forward<T>(obj)) {}

    constexpr bool has_value() const noexcept {
        return m_view.get_underlying() != nullptr;
    }

    constexpr explicit operator bool() const noexcept {
        return has_value();
    }

    constexpr duck_view<Traits...> value() const {
#ifdef __EXCEPTIONS
        if (!has_value()) {
            throw bad_duck_access{"duck_ptr is empty"};
        }
#else
        assert(has_value() && "duck_ptr is empty");
#endif

        return m_view;
    }

    constexpr duck_view<Traits...> operator*() const noexcept {
        assert(has_value());
        return m_view;
    }

    constexpr duck_view<Traits...>* operator->() noexcept {
        assert(has_value());
        return &m_view;
    }

    constexpr const duck_view<Traits...>* operator->() const noexcept {
        assert(has_value());
        return &m_view;
    }
private:
    duck_view<Traits...> m_view;
};

template <is_trait... Traits>
duck_ptr(duck<Traits...>&) -> duck_ptr<Traits...>;

template <is_trait... Traits>
duck_ptr(duck<Traits...>&&) -> duck_ptr<Traits...>;

template <is_trait... Traits>
duck_ptr(const duck<Traits...>&) -> duck_ptr<const Traits...>;

template <is_trait... Traits>
duck_ptr(duck_view<Traits...>) -> duck_ptr<Traits...>;
}

#endif
