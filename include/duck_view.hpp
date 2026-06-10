//
// Created by Ryan on 6/4/2026.
//

#ifndef RJK_DUCK_VIEW_HPP
#define RJK_DUCK_VIEW_HPP

#include "duck.hpp"

namespace rjk {

template <is_trait... Traits>
class duck_view
    : public detail::duck_behavior_base<duck_view<Traits...>, Traits...> {
private:
    using duck_base_t = detail::make_duck_base_t<duck_view, Traits...>;

    constexpr static bool all_const = (std::is_const_v<Traits> && ...);
    using underlying_ptr_t = std::conditional_t<all_const, const void*, void*>;
public:
    template <typename T> requires (
        !std::same_as<std::decay_t<T>, duck_view> &&
        duck_base_t::template meets_tags<T>() &&
        std::is_const_v<std::remove_reference_t<T>> &&
        !all_const
    ) duck_view(T&& obj) = delete("Cannot bind duck_view with mutable traits to a const object");

    template <typename T> requires (!std::same_as<std::decay_t<T>, duck_view> && duck_base_t::template meets_tags<T>())
    duck_view(T&& obj) noexcept
        : m_underlying(std::addressof(obj))
        , m_vtable(&duck_base_t::template static_vtable_for<std::decay_t<T>>)
    { }

    template <typename Duck> requires std::same_as<std::decay_t<Duck>, duck<Traits...>>
    duck_view(Duck&& duck) noexcept
        : m_underlying(duck.get_underlying())
        , m_vtable(duck.get_vtable())
    { }

    template <typename Duck> requires all_const && (
        std::same_as<std::decay_t<Duck>, duck<std::remove_const_t<Traits>...>> ||
        std::same_as<std::decay_t<Duck>, duck_view<std::remove_const_t<Traits>...>>)
    duck_view(Duck&& duck) noexcept
        : m_underlying(duck.get_underlying())
        , m_vtable(duck.get_vtable()->to_const)
    { }

    template <typename T> requires (!std::same_as<std::decay_t<T>, duck_view> &&
                                    duck_base_t::template meets_tags<T>())
    duck_view& operator=(T&& obj) noexcept {
        m_underlying = std::addressof(obj);
        m_vtable = &duck_base_t::template static_vtable_for<std::decay_t<T>>;
        return *this;
    }

    template <std::meta::info VtableMember, duck_tag Tag, detail::fn_qualifiers Qualifiers, typename Func>
    friend class duck_base_t::vtable_function;

    template <typename Derived, is_trait... BaseTraits>
    friend class detail::duck_behavior_base;
private:
    template <typename T>
    bool has_type() const { return m_vtable == &duck_base_t::template static_vtable_for<T>; }

    const auto* get_vtable() const { return m_vtable; }

    void* get_underlying() { return m_underlying; }
    const void* get_underlying() const { return m_underlying; }
private:
    underlying_ptr_t m_underlying;
    const duck_base_t::vtable* m_vtable;
};



}

#endif // RJK_DUCK_VIEW_HPP
