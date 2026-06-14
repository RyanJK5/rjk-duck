//
// Created by Ryan on 6/4/2026.
//

#ifndef RJK_DUCK_BEHAVIOR_BASE_HPP
#define RJK_DUCK_BEHAVIOR_BASE_HPP

#include "duck_base.hpp"

#include <stdexcept>

namespace rjk {
struct bad_duck_access : std::runtime_error {
    bad_duck_access(const char* str) : std::runtime_error(str) {}
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
    friend constexpr decltype(auto) operator++(This&& operand, int) {
        return std::forward<This>(operand)._rjk__lhs_op_plus_plus(0);
    }

    template <typename This>
    requires (duck_base_t::template satisfies_operator<op_minus_minus, This, void>(op_overload_kind::binary_lhs))
    friend constexpr decltype(auto) operator--(This&& operand, int) {
        return std::forward<This>(operand)._rjk__lhs_op_minus_minus(0);
    }
public:
    // operator-> / operator->*: has unique call syntax, must be defined as
    // member functions.

    template <typename This>
    requires (duck_base_t::template satisfies_operator<op_arrow, This, void>(op_overload_kind::unary))
    constexpr decltype(auto) operator->(this This&& operand) {
        return std::forward<This>(operand)._rjk__unary_op_arrow();
    }

    template <typename This, typename Rhs>
    requires (duck_base_t::template satisfies_operator<op_arrow_star, This, Rhs>(op_overload_kind::binary_lhs))
    constexpr decltype(auto) operator->*(this This&& lhs, Rhs&& rhs) {
        return std::forward<This>(lhs)._rjk__lhs_op_arrow_star(std::forward<Rhs>(rhs));
    }

    // operator() / operator[]: can have an arbitrary number of arguments,
    // must be defined as member functions

    template <typename This, typename... Args>
    requires (duck_base_t::template satisfies_operator<op_parentheses, This, void>(op_overload_kind::variadic))
    constexpr decltype(auto) operator()(this This&& operand, Args&&... args) {
        return std::forward<This>(operand)._rjk__op_parentheses(std::forward<Args>(args)...);
    }

    template <typename This, typename... Args>
    requires (duck_base_t::template satisfies_operator<op_square_brackets, This, void>(op_overload_kind::variadic))
    constexpr decltype(auto) operator[](this This&& operand, Args&&... args) {
        return std::forward<This>(operand)._rjk__op_square_brackets(std::forward<Args>(args)...);
    }
public:
    template <typename T, typename Self>
            requires (duck_base_t::template meets_tags<T>())
    constexpr auto* get_if(this Self&& self) noexcept {
        using ret_type = std::conditional_t<
            std::is_const_v<std::remove_reference_t<Self>>,
            const T,
            T
        >;

        if (!self.template has_type<T>()) {
            return static_cast<ret_type*>(nullptr);
        }

        return static_cast<ret_type*>(self.get_underlying());
    }

    template <typename T, typename Self>
        requires (duck_base_t::template meets_tags<T>())
    constexpr decltype(auto) get(this Self&& self) {
        if (!self.template has_type<T>()) {
            constexpr static auto error_str = define_static_string(
                std::string{"duck does not hold '"}
                + display_string_of(^^T) + "'");
            throw bad_duck_access{error_str};
        }

        using obj_type = std::conditional_t<
            std::is_const_v<std::remove_reference_t<Self>>,
            const T, T>;

        return std::forward_like<Self>(*static_cast<obj_type*>(self.get_underlying()));
    }
};

};
}
#endif // RJK_DUCK_DUCK_BEHAVIOR_BASE_HPP
