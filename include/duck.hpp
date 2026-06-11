// (No support for formatting reflection currently)
// clang-format off

#ifndef RJK_DUCK_HPP
#define RJK_DUCK_HPP

#include "duck.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <concepts>
#include <meta>

#include "duck_tags.hpp"
#include "detail/duck_behavior_base.hpp"
#include "detail/storage.hpp"
#include "detail/subsumption_utils.hpp"

namespace rjk {
    template <is_trait... Traits>
    class duck_view;

    template <is_trait... Traits>
    class duck : public detail::duck_behavior_base<duck<Traits...>, Traits...> {
      private:
        using duck_base_t = detail::make_duck_base_t<duck<Traits...>, Traits...>;

        using util = detail::subsumption_utils<Traits...>;

        template <typename T, typename... Args>
        constexpr static bool nothrow_constructor =
            std::is_nothrow_constructible_v<std::decay_t<T>, Args...> &&
            detail::fits_sbo<std::decay_t<T>>;
      public:
        template <typename T> requires (
            !std::same_as<std::decay_t<T>, duck> &&
            !std::same_as<std::decay_t<T>, duck_view<Traits...>> &&
            duck_base_t::template meets_tags<T>())
        explicit duck(T&& obj) noexcept(nothrow_constructor<std::decay_t<T>, T>)
            : duck(detail::init_tag<std::decay_t<T>>{}, std::forward<T>(obj)) {
        }

        template <typename Duck>
        explicit duck(Duck&& d) requires (
            !std::same_as<std::decay_t<Duck>, duck> &&
            util::total_subsumption(decay(^^Duck))
        )
            : m_underlying(d.get_underlying(), d.get_vtable(), std::false_type{})
        { }

        template <typename T, typename... Args> requires (duck_base_t::template meets_tags<T>())
        explicit duck(std::in_place_type_t<T>, Args&&... args) noexcept(nothrow_constructor<std::decay_t<T>, Args...>)
            : duck(detail::init_tag<std::decay_t<T>>{},std::forward<Args>(args)...) { }

        template <typename T, typename U, typename... Args> requires (duck_base_t::template meets_tags<T>())
        explicit duck(std::in_place_type_t<T>, std::initializer_list<U> il, Args&&... args)
            noexcept(nothrow_constructor<std::decay_t<T>, std::initializer_list<U>, Args...>)
            : duck(detail::init_tag<std::decay_t<T>>{}, il, std::forward<Args>(args)...) { }

        template <typename T> requires (!std::same_as<std::decay_t<T>, duck> && duck_base_t::template meets_tags<T>())
        duck& operator=(T&& obj)
            noexcept(nothrow_constructor<std::decay_t<T>, T>) {
            init_from<std::decay_t<T>>(std::forward<T>(obj));
            return *this;
        }

        template <std::meta::info VtableMember, duck_tag Tag, detail::fn_qualifiers Qualifiers, typename Func>
        friend class duck_base_t::vtable_function;

        template <is_trait... DuckTraits>
        friend class duck;

        template <is_trait... ViewTraits>
        friend class duck_view;

        template <typename Derived, is_trait... BaseTraits>
        friend class detail::duck_behavior_base;
      public:
        template <typename T, typename... Args> requires (duck_base_t::template meets_tags<T>())
        std::decay_t<T>& emplace(Args&&... args)
            noexcept(nothrow_constructor<std::decay_t<T>, Args...>) {
            return *init_from<std::decay_t<T>>(std::forward<Args>(args)...);
        }

        template <typename T, typename U, typename... Args> requires (duck_base_t::template meets_tags<T>())
        std::decay_t<T>& emplace(std::initializer_list<U> il, Args&&... args)
            noexcept(nothrow_constructor<std::decay_t<T>, std::initializer_list<U>, Args...>) {
            return *init_from<std::decay_t<T>>(il, std::forward<Args>(args)...);
        }

        template <is_trait... NewTraits, typename Duck>
        friend duck<NewTraits...> narrow_duck(Duck&& src_duck);
      private:
        template <typename T, typename... Args>
        std::decay_t<T>* init_from(Args&&... args) noexcept(nothrow_constructor<std::decay_t<T>, Args...>) {
            m_underlying.template emplace<T>(std::forward<Args>(args)...);
            return static_cast<std::decay_t<T>*>(m_underlying.get());
        }

        template <typename T, typename... Args>
        duck(detail::init_tag<T>, Args&&... args) noexcept(nothrow_constructor<std::decay_t<T>, Args...>)
            : m_underlying(std::in_place_type<T>, std::forward<Args>(args)...)
        { }

        template <typename Duck>
        explicit duck(Duck&& d) requires (util::total_const_subsumption(decay(^^Duck)))
            : m_underlying(
                d.get_underlying(),
                d.get_vtable()->to_const,
                std::bool_constant<std::same_as<std::decay_t<Duck>, duck>>{}
            )
        { }

        template <typename Duck>
        explicit duck(Duck&& d) requires (util::single_trait_subsumption(decay(^^Duck)))
            : m_underlying(
                d.get_underlying(),
                util::template convert_from<Duck>(d.get_vtable()),
                std::bool_constant<std::same_as<std::decay_t<Duck>, duck>>{}
            )
        { }

        const auto* get_vtable() const { return m_underlying.vtable(); }
        
        void* get_underlying() { return m_underlying.get(); }
        const void* get_underlying() const { return m_underlying.get(); }

        template <typename T>
        bool has_type() const { return m_underlying.template has_type<T>(); }
      private:
        detail::storage<typename duck_base_t::vtable_gen_t> m_underlying{};
    };

// Constructs a new duck with the provided traits from the provided src_duck.
// This is intentionally an API hurdle. Though there may be use cases for
// both constraining and copying/moving into a new duck, it's unlikely enough
// that a named function forces the user to acknowledge it's occurring.
template <is_trait... NewTraits, typename Duck>
duck<NewTraits...> narrow_duck(Duck&& src_duck) {
    static_assert(detail::is_duck_type(^^Duck), "Can only narrow a duck or duck_view.");
    // TODO: Add assert that prevents using this for duck<Traits..> / duck_view<Traits...> -> duck<Traits...>
    return duck<NewTraits...>(std::forward<Duck>(src_duck));
}
// Blank, std::any-like duck.
template <typename T, is_trait... Traits> requires (!detail::is_duck_type(^^T))
duck(T&&) -> duck<>;

// Since we only allow total const subsumption, any number of mutable traits
// will deduce to an exact match of the traits.
template <is_trait... Traits>
duck(duck_view<Traits...>) -> duck<Traits...>;

namespace detail {

    // trace_to_duck lets vtable_function access a duck instance
    // without having to store a pointer to it. Each vtable_function
    // is the only member of its respective vtable_function_wrapper
    // class, which is standard layout. This makes the reinterpret_cast
    // below well-defined. Then, since duck inherits from vtable_wrapper,
    // which inherits from each vtable_function_wrapper, the static_cast 
    // is also a well-defined downcast.

    template <typename Derived, duck_tag... Tags>
    template <std::meta::info VtableMember, duck_tag Tag, fn_qualifiers Qualifiers, typename Ret, typename... Args>
    Derived& duck_base<Derived, Tags...>::vtable_function<VtableMember, Tag, Qualifiers, Ret(Args...)>
    ::trace_to_duck() {
        auto* wrapper = reinterpret_cast<vtable_function_wrapper_t*>(this);
        return *static_cast<Derived*>(wrapper);
    }

    template <typename Derived, duck_tag... Tags>
    template <std::meta::info VtableMember, duck_tag Tag, fn_qualifiers Qualifiers, typename Ret, typename... Args>
    const Derived& duck_base<Derived, Tags...>::vtable_function<VtableMember, Tag, Qualifiers, Ret(Args...)>
    ::trace_to_duck() const {
        const auto* wrapper = reinterpret_cast<const vtable_function_wrapper_t*>(this);
        return *static_cast<const Derived*>(wrapper);
    }

    template <typename Derived, duck_tag... Tags>
    template <std::meta::info VtableMember, duck_tag Tag, fn_qualifiers Qualifiers, typename Ret, typename... Args>
    Ret duck_base<Derived, Tags...>::vtable_function<VtableMember, Tag, Qualifiers, Ret(Args...)>
    ::operator()(Args... args) requires (Qualifiers == fn_qualifiers::none) {
        auto& duck = trace_to_duck();
        return duck.get_vtable()->[:VtableMember:](
            duck.get_underlying(),
            std::forward<Args>(args)...
        );
    }

    template <typename Derived, duck_tag... Tags>
    template <std::meta::info VtableMember, duck_tag Tag, fn_qualifiers Qualifiers, typename Ret, typename... Args>
    Ret duck_base<Derived, Tags...>::vtable_function<VtableMember, Tag, Qualifiers, Ret(Args...)>
    ::operator()(Args... args) & requires (Qualifiers == fn_qualifiers::lvalue_ref) {
        auto& duck = trace_to_duck();
        return duck.get_vtable()->[:VtableMember:](
            duck.get_underlying(),
            std::forward<Args>(args)...
        );
    }

    template <typename Derived, duck_tag... Tags>
    template <std::meta::info VtableMember, duck_tag Tag, fn_qualifiers Qualifiers, typename Ret, typename... Args>
    Ret duck_base<Derived, Tags...>::vtable_function<VtableMember, Tag, Qualifiers, Ret(Args...)>
    ::operator()(Args... args) && requires (Qualifiers == fn_qualifiers::rvalue_ref) {
        auto& duck = trace_to_duck();
        return duck.get_vtable()->[:VtableMember:](
            duck.get_underlying(),
            std::forward<Args>(args)...
        );
    }

    template <typename Derived, duck_tag... Tags>
    template <std::meta::info VtableMember, duck_tag Tag, fn_qualifiers Qualifiers, typename Ret, typename... Args>
    Ret duck_base<Derived, Tags...>::vtable_function<VtableMember, Tag, Qualifiers, Ret(Args...)>
    ::operator()(Args... args) const requires (Qualifiers == fn_qualifiers::is_const) {
        const auto& duck = trace_to_duck();
        return duck.get_vtable()->[:VtableMember:](
            duck.get_underlying(),
            std::forward<Args>(args)...
        );
    }

    template <typename Derived, duck_tag... Tags>
    template <std::meta::info VtableMember, duck_tag Tag, fn_qualifiers Qualifiers, typename Ret, typename... Args>
    Ret duck_base<Derived, Tags...>::vtable_function<VtableMember, Tag, Qualifiers, Ret(Args...)>
    ::operator()(Args... args) const &
    requires (Qualifiers == (fn_qualifiers::is_const | fn_qualifiers::lvalue_ref)) {
        const auto& duck = trace_to_duck();
        return duck.get_vtable()->[:VtableMember:](
            duck.get_underlying(),
            std::forward<Args>(args)...
        );
    }

    template <typename Derived, duck_tag... Tags>
    template <std::meta::info VtableMember, duck_tag Tag, fn_qualifiers Qualifiers, typename Ret, typename... Args>
    Ret duck_base<Derived, Tags...>::vtable_function<VtableMember, Tag, Qualifiers, Ret(Args...)>
    ::operator()(Args... args) const && requires (Qualifiers == (fn_qualifiers::is_const | fn_qualifiers::rvalue_ref)) {
        const auto& duck = trace_to_duck();
        return duck.get_vtable()->[:VtableMember:](
            duck.get_underlying(),
            std::forward<Args>(args)...
        );
    }
}
}

#endif