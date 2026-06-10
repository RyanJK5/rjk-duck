#ifndef RJK_DUCK_VIEW_HPP
#define RJK_DUCK_VIEW_HPP

#include "duck.hpp"

namespace rjk {

template <is_trait... Traits>
class duck_view
    : public detail::duck_behavior_base<duck_view<Traits...>, Traits...> {
private:
    using duck_base_t = detail::make_duck_base_t<duck_view, Traits...>;

    constexpr static bool all_const = sizeof...(Traits) > 0 && (std::is_const_v<Traits> && ...);

    using underlying_ptr_t = std::conditional_t<all_const, const void*, void*>;

    using vtable_t = [: std::invoke([] consteval {
        if constexpr (sizeof...(Traits) == 1UZ) {
            return ^^typename detail::trait_vtable_impl<Traits...[0]>::type;
        } else {
            return ^^typename duck_base_t::vtable;
        }
    }) :];

    using mutable_vtable_t = [: std::invoke([] consteval {
        if constexpr (sizeof...(Traits) == 1UZ) {
            return ^^typename detail::trait_vtable_impl<std::remove_const_t<Traits...[0]>>::type;
        } else {
            return ^^void;
        }
    }) :];

    consteval static bool is_duck_type(std::meta::info type) {
        type = dealias(type);

        if (!has_template_arguments(type)) {
            return false;
        }
        if (template_of(type) != ^^duck && template_of(type) != template_of(^^duck_view)) {
            return false;
        }
        return true;
    }

    consteval static bool total_subsumption(std::meta::info type) {
        if (sizeof...(Traits) == 1) {
            return false;
        }
        if (!has_template_arguments(type) || template_of(type) != ^^duck) {
            return false;
        }
        return std::ranges::equal(
            template_arguments_of(^^duck_view),
            template_arguments_of(type)
        );
    }

    consteval static bool total_const_subsumption(std::meta::info type) {
        if (sizeof...(Traits) == 1) {
            return false;
        }
        if (!is_duck_type(type)) {
            return false;
        }
        return std::ranges::equal(
            template_arguments_of(^^duck_view),
            template_arguments_of(type) | std::views::transform(std::meta::add_const)
        );
    }

    consteval static bool single_trait_subsumption(std::meta::info type) {
        if (sizeof...(Traits) != 1) {
            return false;
        }
        if (!is_duck_type(type)) {
            return false;
        }
        return std::ranges::contains(
            template_arguments_of(type),
            template_arguments_of(^^duck_view)[0]
        );
    }

    consteval static bool single_trait_const_subsumption(std::meta::info type) {
        if (sizeof...(Traits) != 1) {
            return false;
        }
        if (!is_duck_type(type)) {
            return false;
        }

        const auto trait = template_arguments_of(^^duck_view)[0];
        if (!is_const(trait)) {
            return false;
        }

        return std::ranges::contains(
            template_arguments_of(type),
            remove_const(trait)
        );
    }
public:
    template <typename T> requires (
        !std::same_as<std::decay_t<T>, duck_view> &&
        duck_base_t::template meets_tags<T>() &&
        std::is_const_v<std::remove_reference_t<T>> &&
        !all_const
    ) duck_view(T&& obj) = delete("Cannot bind duck_view with mutable traits to a const object");

    template <typename T> requires
        (!std::same_as<std::decay_t<T>, duck_view> &&
        duck_base_t::template meets_tags<T>())
    duck_view(T&& obj) noexcept
        : m_underlying(std::addressof(obj))
        , m_vtable(&duck_base_t::template static_vtable_for<std::decay_t<T>>)
    { }

    template <typename Duck>
    duck_view(Duck&& d) noexcept requires (total_subsumption(decay(^^Duck)))
        : m_underlying(d.get_underlying())
        , m_vtable(d.get_vtable())
    { }

    template <typename Duck> requires (total_const_subsumption(decay(^^Duck)))
    duck_view(Duck&& d) noexcept
        : m_underlying(d.get_underlying())
        , m_vtable(d.get_vtable()->to_const)
    { }

    template <typename Duck> requires (single_trait_subsumption(decay(^^Duck)))
    duck_view(Duck&& d) noexcept
        : m_underlying(d.get_underlying())
        , m_vtable(d.get_vtable())
    { }

    template <typename Duck> requires (single_trait_const_subsumption(decay(^^Duck)))
    duck_view(Duck&& d) noexcept
        : m_underlying(d.get_underlying())
        , m_vtable(d.get_vtable()->mutable_vtable_t::to_const)
    { }

    template <std::meta::info VtableMember, duck_tag Tag, detail::fn_qualifiers Qualifiers, typename Func>
    friend class duck_base_t::vtable_function;

    template <typename Derived, is_trait... BaseTraits>
    friend class detail::duck_behavior_base;

    template <is_trait... ViewTraits>
    friend class duck_view;

    template <is_trait... DuckTraits>
    friend class duck;
private:
    template <typename T>
    bool has_type() const { return m_vtable == &duck_base_t::template static_vtable_for<T>; }

    const auto* get_vtable() const { return m_vtable; }

    underlying_ptr_t get_underlying() const { return m_underlying; }
private:
    underlying_ptr_t m_underlying;
    const vtable_t* m_vtable;
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
}

#endif // RJK_DUCK_VIEW_HPP
