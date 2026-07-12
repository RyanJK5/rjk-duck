// clang-format off

#ifndef RJK_DUCK_BEHAVIOR_BASE_HPP
#define RJK_DUCK_BEHAVIOR_BASE_HPP

#include "detail/duck_base.hpp"

#include <cassert>
#include <stdexcept>

namespace rjk {
struct bad_duck_access : std::runtime_error {
    constexpr bad_duck_access(const char* str) : std::runtime_error(str) {}
};

namespace detail {
// duck_behavior_base holds all of the methods that grant duck its functionality,
// such as operator overloading and user-defined vtable functions. Both duck
// and duck_view inherit from this class to avoid code duplication.
template <typename Derived, is_trait... Traits>
class duck_behavior_base
    : protected make_duck_base_t<Derived, Traits...>
    , public make_duck_base_t<Derived, Traits...>::vtable_wrapper {
private:
    using duck_base_t = detail::make_duck_base_t<Derived, Traits...>;

    // most operators are generated as friends via generate_operators.py,
    // since there is currently no way to easily reflect between std::meta::operators
    // and the actual operator functions.
    #include "generated/operator_friends.inl"

    // The following operators are special cases.

    // operator++/operator-- (postfix): explicitly use int as the second argument

    template <typename This>
    requires (duck_base_t::template satisfies_operator<op_plus_plus, This, void>(op_overload_kind::binary_lhs))
    friend constexpr decltype(auto) operator++(This&& operand, int)
    noexcept(noexcept(std::declval<This>()._rjk_lhs_op_plus_plus(0))) {
        return std::forward<This>(operand)._rjk_lhs_op_plus_plus(0);
    }

    template <typename This>
    requires (duck_base_t::template satisfies_operator<op_minus_minus, This, void>(op_overload_kind::binary_lhs))
    friend constexpr decltype(auto) operator--(This&& operand, int)
    noexcept(noexcept(std::declval<This>()._rjk_lhs_op_minus_minus(0))) {
        return std::forward<This>(operand)._rjk_lhs_op_minus_minus(0);
    }
public:
    // operator->: must be defined as member function.
    template <typename This>
    requires (duck_base_t::template satisfies_operator<op_arrow, This, void>(op_overload_kind::unary))
    constexpr decltype(auto) operator->(this This&& operand)
    noexcept(noexcept(std::declval<This>()._rjk_unary_op_arrow())) {
        return std::forward<This>(operand)._rjk_unary_op_arrow();
    }

    // operator() / operator[]: can have an arbitrary number of arguments,
    // must be defined as member functions

    template <typename This, typename... Args>
    requires (duck_base_t::template satisfies_operator<op_parentheses, This, void>(op_overload_kind::variadic))
    constexpr decltype(auto) operator()(this This&& operand, Args&&... args)
    noexcept(noexcept(std::declval<This>()._rjk_op_parentheses(std::declval<Args>()...))) {
        return std::forward<This>(operand)._rjk_op_parentheses(std::forward<Args>(args)...);
    }

    template <typename This, typename... Args>
    requires (duck_base_t::template satisfies_operator<op_square_brackets, This, void>(op_overload_kind::variadic))
    constexpr decltype(auto) operator[](this This&& operand, Args&&... args)
    noexcept(noexcept(std::declval<This>()._rjk_op_square_brackets(std::declval<Args>()...))) {
        return std::forward<This>(operand)._rjk_op_square_brackets(std::forward<Args>(args)...);
    }
};

};

template <typename T, typename Duck>
    requires (detail::is_duck_type(^^Duck))
constexpr auto* get_if(Duck* self) noexcept {
    static_assert(std::decay_t<Duck>::duck_base_t::template meets_tags<T>());
    assert(self != nullptr && "cannot pass nullptr into rjk::get_if");

    using ret_type = std::conditional_t<
        std::is_const_v<std::remove_reference_t<Duck>>,
        const T,
        T
    >;

    if (!self->template has_type<T>()) {
        return static_cast<ret_type*>(nullptr);
    }

    return static_cast<ret_type*>(self->get_underlying());
}

template <typename T, typename Duck>
    requires (detail::is_duck_type(^^Duck))
constexpr decltype(auto) get(Duck&& self) {
    static_assert(std::decay_t<Duck>::duck_base_t::template meets_tags<T>());

    constexpr static auto error_str = define_static_string(
        std::string{"duck does not hold '"}
        + display_string_of(^^T) + "'");
#ifdef __EXCEPTIONS
    if (!self.template has_type<T>()) {
        throw bad_duck_access{error_str};
    }
#else
    assert(self.template has_type<T>() && error_str);
#endif

    using obj_type = std::conditional_t<
        std::is_const_v<std::remove_reference_t<Duck>>,
        const T, T>;

    return std::forward_like<Duck>(*static_cast<obj_type*>(self.get_underlying()));
}

template <typename Duck> requires (detail::is_duck_type(^^Duck))

#ifdef __cpp_rtti
constexpr const std::type_info& typeid_of(Duck&& d) noexcept {
    return *d.get_vtable()->typeid_of;
}
#else
constexpr const std::type_info& typeid_of(Duck&& d) noexcept
    = delete("typeid_of not permitted with -fno-rtti");
#endif

}
#endif
