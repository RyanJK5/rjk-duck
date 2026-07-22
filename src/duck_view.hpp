// clang-format off

#ifndef RJK_DUCK_VIEW_HPP
#define RJK_DUCK_VIEW_HPP

#include "duck.hpp"
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

    using util = detail::subsumption_utils<duck_view, Traits...>;
public:
    template <typename T> requires
        (!detail::duck_type<T> &&
        duck_base_t::template meets_tags<T>() &&
        !std::is_function_v<std::remove_pointer_t<std::decay_t<T>>> &&
        (all_const || !std::is_const_v<std::remove_reference_t<T>>))
    constexpr duck_view(T&& obj) noexcept
        : m_underlying(std::addressof(obj))
        , m_caller(&duck_base_t::template static_vtable_for<std::remove_cvref_t<T>>)
    { }

    template <typename Duck> requires (util::template can_convert_from<Duck>)
    constexpr duck_view(Duck&& d) noexcept
        : m_underlying(d.get_underlying())
        , m_caller(util::template convert_from<Duck>(d.get_vtable()))
    { }

    template <std::meta::info VtableMember, duck_tag Tag, detail::fn_qualifiers Qualifiers, typename Func>
    friend class duck_base_t::vtable_function;

    template <typename T, typename Duck> requires detail::valid_duck_and_type<T, Duck>
    friend constexpr auto* get_if(Duck* d) noexcept;

    template <typename T, typename Duck> requires detail::valid_duck_and_type<T, Duck>
    friend constexpr decltype(auto) get(Duck&& d);

    template <detail::duck_type Duck>
    friend constexpr const std::type_info& typeid_of(const Duck& d) noexcept;

    template <is_trait... ViewTraits>
    friend class duck_view;

    template <is_trait... DuckTraits>
    friend class duck;

    friend class duck_ptr<Traits...>;
private:
    constexpr duck_view() noexcept : m_underlying(nullptr), m_caller(nullptr) { }

    template <typename T>
    constexpr bool has_type() const noexcept { return get_vtable() == &duck_base_t::template static_vtable_for<T>; }

    constexpr const auto& get_callable() const noexcept { return m_caller; }
    constexpr const auto* get_vtable() const noexcept { return m_caller.get_vtable(); }

    constexpr underlying_ptr_t get_underlying() const noexcept { return m_underlying; }
private:
    underlying_ptr_t m_underlying;
    detail::vtable_caller<typename duck_base_t::vtable_gen_t> m_caller;
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

    constexpr void reset() noexcept {
        m_view = {};
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
