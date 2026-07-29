// (No support for formatting reflection currently)
// clang-format off

#ifndef RJK_DUCK_HPP
#define RJK_DUCK_HPP

#include <algorithm>
#include <array>
#include <concepts>
#include <meta>

#include "duck_tags.hpp"
#include "detail/duck_behavior_base.hpp"
#include "detail/storage.hpp"
#include "detail/subsumption_utils.hpp"

namespace rjk {
    template <typename... Traits> requires detail::valid_trait_set<Traits...>
    class duck : public detail::duck_behavior_base<duck<Traits...>> {
      private:
        using duck_base_t = detail::make_duck_base_t<duck>;
        using util = duck_base_t::util;
        using storage_t = detail::storage<typename duck_base_t::vtable_gen_t>;

        template <typename T, typename... Args>
        constexpr static bool nothrow_constructor =
            std::is_nothrow_constructible_v<std::decay_t<T>, Args...> &&
            storage_t::template fits_sbo<std::decay_t<T>>;

        template <typename TraitRet, typename ActualRet>
        friend consteval bool detail::is_conversion_noexcept_impl();
      public:
        using allocator_type = storage_t::allocator_type;

        template <typename T> requires (
            !detail::duck_type<T> &&
            !duck_base_t::template meets_tags<T>)
        constexpr duck(T&& obj) = delete("'T' does not satisfy 'Traits...'");

        // Constructor from object
        template <typename T> requires (
            !detail::duck_type<T> &&
            duck_base_t::template meets_tags<T> &&
            std::default_initializable<allocator_type>)
        constexpr explicit duck(T&& obj) noexcept(nothrow_constructor<T, T>)
            : m_underlying(allocator_type{}, std::in_place_type<std::decay_t<T>>, std::forward<T>(obj))
        { }

        // Allocator constructor from object
        template <typename T> requires (
            !detail::duck_type<T> &&
            duck_base_t::template meets_tags<T>)
        constexpr explicit duck(std::allocator_arg_t,
            const allocator_type& alloc, T&& obj) noexcept(nothrow_constructor<T, T>)
            : m_underlying(alloc, std::in_place_type<std::decay_t<T>>, std::forward<T>(obj))
        { }

        // Allocator copy constructor
        constexpr duck(std::allocator_arg_t, const allocator_type& alloc, const duck& other)
            : m_underlying(alloc, other.m_underlying)
        { }

        // Allocator move constructor
        constexpr duck(std::allocator_arg_t, const allocator_type& alloc, duck&& other)
            : m_underlying(alloc, std::move(other.m_underlying))
        { }

        // Constructor from reordered duck_view
        template <typename Duck> requires (
            detail::is_duck_view(^^Duck) &&
            util::template is_permutation<Duck> &&
            std::default_initializable<allocator_type>)
        constexpr explicit duck(Duck&& d)
            : m_underlying(d.get_underlying(), d.get_vtable())
        { }

        // Allocator constructor from reordered duck_view
        template <typename Duck> requires (
            detail::is_duck_view(^^Duck) &&
            util::template is_permutation<Duck>)
        constexpr explicit duck(std::allocator_arg_t, const allocator_type& alloc, Duck&& d)
            : m_underlying(d.get_underlying(), d.get_vtable(), alloc)
        { }

        // Constructor from reordered duck
        template <typename Duck> requires (
            detail::is_duck_container(^^Duck) &&
            util::template is_permutation<Duck>)
        constexpr explicit duck(Duck&& d)
            noexcept(noexcept(storage_t{std::forward_like<Duck>(std::declval<storage_t&>())}))
            : m_underlying(std::forward_like<Duck>(d.m_underlying))
        { }

        // Allocator constructor from reordered duck_view
        template <typename Duck> requires (
            detail::is_duck_container(^^Duck) &&
            util::template is_permutation<Duck>)
        constexpr explicit duck(std::allocator_arg_t, const allocator_type& alloc, Duck&& d)
            noexcept(noexcept(storage_t{
                std::forward_like<Duck>(std::declval<storage_t&>()),
                std::declval<const allocator_type&>()
            }))
            : m_underlying(std::forward_like<Duck>(d.m_underlying), alloc)
        { }

        template <typename T, typename... Args> requires (!duck_base_t::template meets_tags<T>)
        constexpr explicit duck(std::in_place_type_t<T>, Args&&... args)
            = delete("'T' does not satisfy 'Traits...'");

        // In-place constructor
        template <typename T, typename... Args>  requires (
            duck_base_t::template meets_tags<T> &&
            std::default_initializable<allocator_type>)
        constexpr explicit duck(std::in_place_type_t<T>, Args&&... args) noexcept(nothrow_constructor<T, Args...>)
            : m_underlying(allocator_type{}, std::in_place_type<T>, std::forward<Args>(args)...) { }

        // Allocator in-place constructor
        template <typename T, typename... Args>  requires (duck_base_t::template meets_tags<T>)
        constexpr explicit duck(std::allocator_arg_t, const allocator_type& alloc,
            std::in_place_type_t<T>, Args&&... args) noexcept(nothrow_constructor<T, Args...>)
            : m_underlying(alloc, std::in_place_type<T>, std::forward<Args>(args)...) { }

        template <typename T, typename U, typename... Args> requires (!duck_base_t::template meets_tags<T>)
        constexpr explicit duck(std::in_place_type_t<T>, std::initializer_list<U> il, Args&&... args)
            = delete("'T' does not satisfy 'Traits...'");

        // Init list constructor
        template <typename T, typename U, typename... Args> requires (duck_base_t::template meets_tags<T>)
        constexpr explicit duck(std::in_place_type_t<T>, std::initializer_list<U> il, Args&&... args)
            noexcept(nothrow_constructor<T, std::initializer_list<U>, Args...>)
            : m_underlying(allocator_type{}, std::in_place_type<T>, il, std::forward<Args>(args)...) { }

        // Allocator-aware init list constructor
        template <typename T, typename U, typename... Args> requires (duck_base_t::template meets_tags<T>)
        constexpr explicit duck(std::allocator_arg_t, const allocator_type& alloc,
            std::in_place_type_t<T>, std::initializer_list<U> il, Args&&... args)
            noexcept(nothrow_constructor<T, std::initializer_list<U>, Args...>)
            : m_underlying(alloc, std::in_place_type<T>, il, std::forward<Args>(args)...) { }

        template <typename T> requires
            (!std::same_as<std::decay_t<T>, duck> &&
            !duck_base_t::template meets_tags<T>)
        constexpr duck& operator=(T&& obj) = delete("'T' does not satisfy 'Traits...'");

        template <typename T> requires (!std::same_as<std::decay_t<T>, duck> && duck_base_t::template meets_tags<T>)
        constexpr duck& operator=(T&& obj) noexcept(nothrow_constructor<T, T>) {
            init_from<std::decay_t<T>>(std::forward<T>(obj));
            return *this;
        }

        template <std::meta::info VtableMember, duck_tag Tag, detail::fn_qualifiers Qualifiers, typename Func>
        friend class duck_base_t::vtable_function;

        template <typename... OtherTraits> requires detail::valid_trait_set<OtherTraits...>
        friend class duck;

        template <typename... OtherTraits> requires detail::valid_trait_set<OtherTraits...>
        friend class duck_view;

        template <typename T, typename Duck>
            requires detail::valid_duck_and_type<T, Duck>
        friend constexpr auto* get_if(Duck* d) noexcept;

        template <typename T, typename Duck>
            requires detail::valid_duck_and_type<T, Duck>
        friend constexpr decltype(auto) get(Duck&& d);

        template <detail::duck_type Duck>
        friend constexpr const std::type_info& typeid_of(const Duck& d) noexcept;
      public:
        // TODO: Constrain emplace to not include duck_view

        template <typename T, typename Duck, typename... Args>
            requires detail::valid_duck_and_type<T, Duck>
        friend constexpr T& emplace(Duck&& d, Args&&... args)
            noexcept(std::decay_t<Duck>::template nothrow_constructor<T, Args...>);

        template <typename T, typename U, typename Duck, typename... Args>
            requires detail::valid_duck_and_type<T, Duck>
        friend constexpr T& emplace(Duck&& d, std::initializer_list<U> il, Args&&... args)
            noexcept(std::decay_t<Duck>::template nothrow_constructor<T, std::initializer_list<U>, Args...>);

        template <typename Duck> requires (detail::is_duck_container(^^Duck))
        friend constexpr typename std::decay_t<Duck>::allocator_type get_allocator(const Duck& d) noexcept;

        template <typename... NewTraits, detail::duck_type Duck>
        friend duck<NewTraits...> make_narrowed(Duck&& src_duck)
            noexcept(noexcept(duck<NewTraits...>{std::declval<Duck>()}));

        template <typename... NewTraits, detail::duck_type Duck>
        friend duck<NewTraits...> make_narrowed(Duck&& src_duck, const typename duck<NewTraits...>::allocator_type& alloc)
            noexcept(noexcept(duck<NewTraits...>{std::declval<Duck>(), std::declval<const typename duck<NewTraits...>::allocator_type&>()}));
      private:
        template <typename T, typename... Args>
        constexpr T* init_from(Args&&... args) noexcept(nothrow_constructor<T, Args...>) {
            m_underlying.template emplace<T>(std::forward<Args>(args)...);
            return static_cast<T*>(m_underlying.get());
        }

        // Narrowing constructor from duck_view
        template <typename Duck> requires (
            detail::is_duck_view(^^Duck) &&
            !util::template is_permutation<Duck> &&
            util::template can_convert_from<Duck> &&
            std::default_initializable<allocator_type>)
        constexpr explicit duck(Duck&& d)
            : duck(std::forward<Duck>(d), allocator_type{})
        { }

        // Allocator narrowing constructor from duck_view
        template <typename Duck> requires (
            detail::is_duck_view(^^Duck) &&
            !util::template is_permutation<Duck> &&
            util::template can_convert_from<Duck>)
        constexpr explicit duck(Duck&& d, const allocator_type& alloc)
            : m_underlying(d.get_underlying(), util::template convert_from<Duck>(d.get_vtable()), alloc)
        { }

        // Narrowing constructor from duck
        template <typename Duck> requires (
            detail::is_duck_container(^^Duck) &&
            !util::template is_permutation<Duck> &&
            util::template can_convert_from<Duck>)
        constexpr explicit duck(Duck&& d)
            noexcept(noexcept(storage_t{
                std::forward_like<Duck>(std::declval<storage_t&>()),
                util::template convert_from<Duck>(std::declval<Duck>().get_vtable())
            }))
            : m_underlying(
                std::forward_like<Duck>(d.m_underlying),
                util::template convert_from<Duck>(d.get_vtable()))
        { }

        // Allocator narrowing constructor from duck
        template <typename Duck> requires (
            detail::is_duck_container(^^Duck) &&
            !util::template is_permutation<Duck> &&
            util::template can_convert_from<Duck>)
        constexpr explicit duck(Duck&& d, const allocator_type& alloc)
            noexcept(noexcept(storage_t{
                std::forward_like<Duck>(std::declval<storage_t&>()),
                util::template convert_from<Duck>(std::declval<Duck>().get_vtable()),
                std::declval<const allocator_type&>()
            }))
            : m_underlying(
                std::forward_like<Duck>(d.m_underlying),
                util::template convert_from<Duck>(d.get_vtable()),
                alloc)
        { }

        constexpr const auto& get_callable() const noexcept { return m_underlying.callable(); }
        constexpr const auto* get_vtable() const noexcept { return m_underlying.get_vtable(); }
        
        constexpr void* get_underlying() noexcept { return m_underlying.get(); }
        constexpr const void* get_underlying() const noexcept { return m_underlying.get(); }

        template <typename T>
        constexpr bool has_type() const noexcept { return m_underlying.template has_type<T>(); }
      private:
        storage_t m_underlying{};
    };

// Constructs a new duck with the provided traits from the provided src_duck.
// This is intentionally an API hurdle. Though there may be use cases for
// both constraining and copying/moving into a new duck, it's unlikely enough
// that a named function forces the user to acknowledge it's occurring.
template <typename... NewTraits, detail::duck_type Duck>
duck<NewTraits...> make_narrowed(Duck&& src_duck)
noexcept(noexcept(duck<NewTraits...>{std::declval<Duck>()})) {
    return duck<NewTraits...>{std::forward<Duck>(src_duck)};
}

template <typename... NewTraits, detail::duck_type Duck>
duck<NewTraits...> make_narrowed(Duck&& src_duck, const typename duck<NewTraits...>::allocator_type& alloc)
noexcept(noexcept(duck<NewTraits...>{std::declval<Duck>(), std::declval<const typename duck<NewTraits...>::allocator_type&>()})) {
    return duck<NewTraits...>{std::forward<Duck>(src_duck), alloc};
}

template <typename T, typename Duck, typename... Args>
    requires detail::valid_duck_and_type<T, Duck>
constexpr T& emplace(Duck&& d, Args&&... args)
    noexcept(std::decay_t<Duck>::template nothrow_constructor<T, Args...>) {
    return *d.template init_from<T>(std::forward<Args>(args)...);
}

template <typename T, typename U, typename Duck, typename... Args>
    requires detail::valid_duck_and_type<T, Duck>
constexpr T& emplace(Duck&& d, std::initializer_list<U> il, Args&&... args)
    noexcept(std::decay_t<Duck>::template nothrow_constructor<T, std::initializer_list<U>, Args...>) {
    return *d.template init_from<T>(il, std::forward<Args>(args)...);
}

template <typename Duck> requires (detail::is_duck_container(^^Duck))
constexpr typename std::decay_t<Duck>::allocator_type get_allocator(const Duck& d) noexcept {
    return d.m_underlying.get_allocator();
}

// Blank, std::any-like duck.
template <typename T, typename... Traits> requires (!detail::duck_type<T>)
duck(T&&) -> duck<>;

template <typename... Traits>
duck(duck_view<Traits...>) -> duck<Traits...>;

namespace detail {

    // trace_to_duck lets vtable_function access a duck instance
    // without having to store a pointer to it. Each vtable_function
    // is the only member of its respective vtable_function_wrapper
    // class, which is standard layout. This makes the reinterpret_cast
    // below well-defined. Then, since duck inherits from vtable_wrapper,
    // which inherits from each vtable_function_wrapper, the static_cast 
    // is also a well-defined downcast.

    // We can't use reinterpret_cast at compile time, but can accomplish
    // essentially the same thing by casting to void first in the consteval
    // branch.

    template <typename Derived, typename... Traits>
    template <std::meta::info VtableMember, duck_tag Tag, fn_qualifiers Qualifiers, bool Noexcept, typename Ret, typename... Args>
    constexpr Derived& duck_base<Derived, Traits...>::vtable_function<VtableMember, Tag, Qualifiers, Ret(Args...) noexcept(Noexcept)>
    ::trace_to_duck() noexcept {
        if consteval {
            void* voided = this;
            auto* wrapper = static_cast<vtable_function_wrapper_t*>(voided);
            return *static_cast<Derived*>(wrapper);
        } else {
            auto* wrapper = reinterpret_cast<vtable_function_wrapper_t*>(this);
            return *static_cast<Derived*>(wrapper);
        }
    }

    template <typename Derived, typename... Traits>
    template <std::meta::info VtableMember, duck_tag Tag, fn_qualifiers Qualifiers, bool Noexcept, typename Ret, typename... Args>
    constexpr const Derived& duck_base<Derived, Traits...>::vtable_function<VtableMember, Tag, Qualifiers, Ret(Args...) noexcept(Noexcept)>
    ::trace_to_duck() const noexcept {
        if consteval {
            const void* voided = this;
            const auto* wrapper = static_cast<const vtable_function_wrapper_t*>(voided);
            return *static_cast<const Derived*>(wrapper);
        } else {
            const auto* wrapper = reinterpret_cast<const vtable_function_wrapper_t*>(this);
            return *static_cast<const Derived*>(wrapper);
        }
    }

    template <typename Derived, typename... Traits>
    template <std::meta::info VtableMember, duck_tag Tag, fn_qualifiers Qualifiers, bool Noexcept, typename Ret, typename... Args>
    constexpr Ret duck_base<Derived, Traits...>::vtable_function<VtableMember, Tag, Qualifiers, Ret(Args...) noexcept(Noexcept)>
    ::operator()(Args... args) noexcept(Noexcept) requires (Qualifiers == fn_qualifiers::none) {
        auto& duck = trace_to_duck();
        return duck.get_callable().template call<VtableMember, Noexcept>(
            duck.get_underlying(),
            std::forward<Args>(args)...
        );
    }

    template <typename Derived, typename... Traits>
    template <std::meta::info VtableMember, duck_tag Tag, fn_qualifiers Qualifiers, bool Noexcept, typename Ret, typename... Args>
    constexpr Ret duck_base<Derived, Traits...>::vtable_function<VtableMember, Tag, Qualifiers, Ret(Args...) noexcept(Noexcept)>
    ::operator()(Args... args) & noexcept(Noexcept) requires (Qualifiers == fn_qualifiers::lvalue_ref) {
        auto& duck = trace_to_duck();
        return duck.get_callable().template call<VtableMember, Noexcept>(
            duck.get_underlying(),
            std::forward<Args>(args)...
        );
    }

    template <typename Derived, typename... Traits>
    template <std::meta::info VtableMember, duck_tag Tag, fn_qualifiers Qualifiers, bool Noexcept, typename Ret, typename... Args>
    constexpr Ret duck_base<Derived, Traits...>::vtable_function<VtableMember, Tag, Qualifiers, Ret(Args...) noexcept(Noexcept)>
    ::operator()(Args... args) && noexcept(Noexcept) requires (Qualifiers == fn_qualifiers::rvalue_ref) {
        auto& duck = trace_to_duck();
        return duck.get_callable().template call<VtableMember, Noexcept>(
            duck.get_underlying(),
            std::forward<Args>(args)...
        );
    }

    template <typename Derived, typename... Traits>
    template <std::meta::info VtableMember, duck_tag Tag, fn_qualifiers Qualifiers, bool Noexcept, typename Ret, typename... Args>
    constexpr Ret duck_base<Derived, Traits...>::vtable_function<VtableMember, Tag, Qualifiers, Ret(Args...) noexcept(Noexcept)>
    ::operator()(Args... args) const noexcept(Noexcept) requires (Qualifiers == fn_qualifiers::is_const) {
        const auto& duck = trace_to_duck();
        return duck.get_callable().template call<VtableMember, Noexcept>(
            duck.get_underlying(),
            std::forward<Args>(args)...
        );
    }

    template <typename Derived, typename... Traits>
    template <std::meta::info VtableMember, duck_tag Tag, fn_qualifiers Qualifiers, bool Noexcept, typename Ret, typename... Args>
    constexpr Ret duck_base<Derived, Traits...>::vtable_function<VtableMember, Tag, Qualifiers, Ret(Args...) noexcept(Noexcept)>
    ::operator()(Args... args) const & noexcept(Noexcept) requires (Qualifiers == (fn_qualifiers::is_const | fn_qualifiers::lvalue_ref)) {
        const auto& duck = trace_to_duck();
        return duck.get_callable().template call<VtableMember, Noexcept>(
            duck.get_underlying(),
            std::forward<Args>(args)...
        );
    }

    template <typename Derived, typename... Traits>
    template <std::meta::info VtableMember, duck_tag Tag, fn_qualifiers Qualifiers, bool Noexcept, typename Ret, typename... Args>
    constexpr Ret duck_base<Derived, Traits...>::vtable_function<VtableMember, Tag, Qualifiers, Ret(Args...) noexcept(Noexcept)>
    ::operator()(Args... args) const && noexcept(Noexcept) requires (Qualifiers == (fn_qualifiers::is_const | fn_qualifiers::rvalue_ref)) {
        const auto& duck = trace_to_duck();
        return duck.get_callable().template call<VtableMember, Noexcept>(
            duck.get_underlying(),
            std::forward<Args>(args)...
        );
    }
}
}

#endif
