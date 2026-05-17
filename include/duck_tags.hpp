#pragma once

#include "detail/fixed_string.hpp"
#include "detail/fn_traits.hpp"
#include "detail/remove_fn_qualifiers.hpp"
#include "detail/remove_noexcept.hpp"
#include "detail/substitute_fn_traits.hpp"

namespace rjk {
namespace like_options {
struct data_members {};

struct member_functions {};

struct conversions {};

struct operators {};
}

template <typename T>
concept like_option = [] consteval {
    return parent_of(^^T) == ^ ^ ::rjk::like_options;
}();
}

namespace rjk {
template <typename T>
concept function_signature = std::is_function_v<T>;

using enum std::meta::operators;

template <typename E, bool Enumerable = std::meta::is_enumerable_type(^^E)>
    requires std::is_enum_v<E>
constexpr std::string_view enum_to_string(E value) {
    if constexpr (Enumerable) {
        template for (constexpr auto e : std::define_static_array(
            std::meta::enumerators_of(^^E))) {
            if (value == [:e:]) {
                return std::meta::identifier_of(e);
            }
        }
    }
    return "<unnamed>";
}

inline namespace tags {
template <fixed_string Identifier, function_signature Func>
struct has_fn {};

template <fixed_string Identifier, typename T>
struct has_member {};

template <typename T>
struct converts_to {};

template <typename T, like_option... Options>
struct like {};

template <std::meta::operators Operator, function_signature Func>
struct has_op {};

}

struct self {};

struct other {};

template <typename T>
concept duck_tag = [] consteval {
    return parent_of(^^T) == ^ ^ ::rjk::tags;
}();

template <typename Type, std::meta::info Tag>
consteval bool satisfies_fn_tag() {
    static_assert(is_class_type(^^Type));

    const auto type_members = members_of(
^^Type, std::meta::access_context::current());
    constexpr static auto name = std::string_view{
        [:template_arguments_of(Tag)[0]:]};
    constexpr static auto sig = remove_noexcept(template_arguments_of(Tag)[1]);

    bool found = false;
    for (auto member : type_members) {
        if (has_identifier(member) &&
            identifier_of(member) == name &&
            is_function(member) &&
            remove_noexcept(type_of(member)) == sig
        ) {
            found = true;
            break;
        }
    }
    return found;
}

template <typename T, typename Arg, typename Ret>
concept addable = requires(T t, Arg a)
{
    { t + a } -> std::same_as<Ret>;
};

template <std::meta::operators Op, duck_tag... Tags>
consteval bool has_operator_tag() {
    template for (constexpr auto tag : {^^Tags...}) {
        if constexpr (template_of(tag) == ^ ^ has_op) {
            if constexpr ([:template_arguments_of(tag)[0]

            :
            ]
            ==
            Op
            )
            {
                return true;
            }
        }
    }
    return false;
}

template <std::meta::operators Op, typename Operand, typename Ret>
consteval bool check_unary_op() {
    if constexpr (Op == op_plus)
        return requires(Operand o) { { +o } -> std::same_as<Ret>; };
    if constexpr (Op == op_minus)
        return requires(Operand o) { { -o } -> std::same_as<Ret>; };
    return false;
}

template <std::meta::operators Op, typename Lhs, typename Rhs, typename Ret>
consteval bool check_op() {
    if constexpr (Op == op_plus)
        return requires(Lhs l, Rhs r) { { l + r } -> std::same_as<Ret>; };
    if constexpr (Op == op_minus)
        return requires(Lhs l, Rhs r) { { l - r } -> std::same_as<Ret>; };
    return false;
}

template <typename Type, std::meta::info Tag>
consteval bool satisfies_op_tag() {
    using substituted_func =

    [: remove_fn_qualifiers(detail::substitute_fn_args(
        detail::substitute_fn_args(
            template_arguments_of(Tag)[1],
            ^^other,
            ^^Type
        ),
        ^^self,
        ^^Type
    )) :];
    using ret = fn_return_type_t<substituted_func>;

    using arg1 = fn_arg_t<substituted_func, 0>;
    constexpr static auto tag_op = [:template_arguments_of(Tag)[0]

    :
    ]
    ;
    if constexpr (fn_arg_count_v<substituted_func> == 1) {
        return check_unary_op<tag_op, arg1, ret>();
    } else {
        using arg2 = fn_arg_t<substituted_func, 1>;
        return check_op<tag_op, arg1, arg2, ret>();
    }
}

template <typename Type, typename... Tags>
concept satisfies_tags = [] consteval {
    template for (constexpr auto tag : {^^Tags...}) {
        if constexpr (template_of(tag) == ^ ^ has_fn) {
            if constexpr (satisfies_fn_tag<Type, tag>()) {
                continue;
            }
        }
        if constexpr (template_of(tag) == ^ ^ has_op) {
            if constexpr (satisfies_op_tag<Type, tag>()) {
                continue;
            }
        }
        return false;
    }
    return true;
}();
}