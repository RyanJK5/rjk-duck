// clang-format off
#ifndef RJK_DUCK_TAGS_HPP
#define RJK_DUCK_TAGS_HPP

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
    return parent_of(^^T) == ^^::rjk::like_options;
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
        template for (constexpr auto e : std::define_static_array(std::meta::enumerators_of(^^E))) {
            if (value == [:e:]) {
                return std::meta::identifier_of(e);
            }
        }
    } else {
        return "<unnamed>";
    }
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

// Used for denoting the relative location of two ducks in a has_op signature.
struct self {};
struct this_duck_t {};

// Can be plugged into rjk::duck.
template <typename T>
concept duck_tag = [] consteval {
    return parent_of(^^T) == ^^::rjk::tags;
}();

template <typename Type, std::meta::info Tag>
consteval bool satisfies_fn_tag() {
    static_assert(is_class_type(^^Type));

    const auto type_members = members_of(^^Type, std::meta::access_context::current());
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

enum struct op_overload_kind {
    any_kind,
    binary_lhs,
    binary_rhs,
    unary
};

template <std::meta::operators Op, duck_tag... Tags>
consteval bool has_operator_tag(op_overload_kind kind = op_overload_kind::any_kind) {
    template for (constexpr auto tag : {^^Tags...}) {
        if constexpr (template_of(tag) == ^^has_op) {
            if constexpr ([:template_arguments_of(tag)[0]:] != Op) {
                continue;
            } else {
                constexpr static auto full_sig = template_arguments_of(tag)[1];
                constexpr static bool self_is_lhs = remove_cvref(substitute(^^fn_arg_t, {full_sig, std::meta::reflect_constant(0)})) == ^^self;
                constexpr static auto after_remove_self = detail::remove_arg(full_sig, ^^self);
                constexpr static bool is_unary = extract<std::size_t>(substitute(^^fn_arg_count_v, {remove_fn_qualifiers(after_remove_self)})) == 0;

                switch (kind) {
                    using enum op_overload_kind;
                case any_kind:   return true;
                case binary_lhs: if (!is_unary && self_is_lhs) return true; continue;
                case binary_rhs: if (!is_unary && !self_is_lhs) return true; continue;
                case unary:      if (is_unary) return true; continue;

                }
            }
        }
    }
    return false;
}

template <typename Operand, typename Ret>
consteval bool check_unary_op(std::meta::operators op) {
    switch (op) {
    case op_plus: return requires(Operand o) { { +o } -> std::same_as<Ret>; };
    case op_minus: return requires(Operand o) { { -o } -> std::same_as<Ret>; };
    default: return false;
    }
}

template <typename Lhs, typename Rhs, typename Ret>
consteval bool check_op(std::meta::operators op) {
    switch (op) {
    case op_plus: return requires(Lhs l, Rhs r) { { l + r } -> std::same_as<Ret>; };
    case op_minus:  return requires(Lhs l, Rhs r) { { l - r } -> std::same_as<Ret>; };
    default: return false;
    }
}

template <typename Type, typename DuckType, std::meta::info Tag>
consteval bool satisfies_op_tag() {
        using substituted_func = [: remove_fn_qualifiers(detail::substitute_fn_args(
        detail::substitute_fn_args(
            template_arguments_of(Tag)[1],
            ^^this_duck_t,
            ^^DuckType
        ),
        ^^self,
        ^^Type
    )) :];
    using ret = fn_return_type_t<substituted_func>;

    using arg1 = fn_arg_t<substituted_func, 0>;
    constexpr static auto tag_op = [: template_arguments_of(Tag)[0] :];
    if constexpr (fn_arg_count_v<substituted_func> == 1) {
        return check_unary_op<arg1, ret>(tag_op);
    } else {
        using arg2 = fn_arg_t<substituted_func, 1>;
        return check_op<arg1, arg2, ret>(tag_op);
    }
}

template <typename Type, typename DuckType, typename... Tags>
consteval bool satisfies_tags() {
    template for (constexpr auto tag : {^^Tags...}) {
        if constexpr (template_of(tag) == ^^has_fn) {
            if constexpr (satisfies_fn_tag<Type, tag>()) {
                continue;
            }
        }
        if constexpr (template_of(tag) == ^^has_op) {
            if constexpr (satisfies_op_tag<Type, DuckType, tag>()) {
                continue;
            }
        }
        return false;
    }
    return true;
}

}

#endif