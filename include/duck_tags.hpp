// clang-format off
#ifndef RJK_DUCK_TAGS_HPP
#define RJK_DUCK_TAGS_HPP

#include "generated/do_unary_op.hpp"
#include "generated/do_binary_op.hpp"
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

// Used to denote another duck of the same type. For example:
// using MyDuck = rjk::duck<rjk::has_fn<"read_from", int(const rjk::duck_t&)>>;
// MyDuck x{Foo{}};
// MyDuck y{Foo{}};
// x.read_from(y);
struct duck_t {};

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

    return std::ranges::any_of(type_members, [](auto member) {
        return (has_identifier(member) &&
            identifier_of(member) == name &&
            is_function(member) &&
            remove_noexcept(type_of(member)) == sig
        );
    });
}

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

template <typename Type, typename DuckType, std::meta::info Tag>
consteval std::optional<std::meta::info> make_substitution() {
    constexpr static auto base_signature = template_arguments_of(Tag)[1];
    using BaseSignatureType = [: base_signature :];
    constexpr static auto self_count = count_args_of_type<BaseSignatureType>(^^self);
    if constexpr (self_count == 0) {
        return std::nullopt;
    }
    else if constexpr(self_count > 1) {
        throw std::logic_error("Only one 'self' argument is allowed");
    }

    return remove_fn_qualifiers(
        detail::substitute_fn_args(
            detail::substitute_fn_args(base_signature, ^^self, ^^Type),
            ^^duck_t, ^^DuckType
        )
    );
}

template <typename Type, typename DuckType, std::meta::info Tag>
consteval bool satisfies_op_tag() {
    constexpr static auto substitution = make_substitution<Type, DuckType, Tag>();
    if constexpr (!substitution.has_value()) {
        return false;
    }
    using substituted_func = [: *substitution :];
    using ret = fn_return_type_t<substituted_func>;
    using arg1 = fn_arg_t<substituted_func, 0>;

    constexpr static auto tag_op = [: template_arguments_of(Tag)[0] :];
    if constexpr (fn_arg_count_v<substituted_func> == 1) {
        return requires(arg1 operand) {
            { do_unary_op<tag_op>(operand) } -> std::same_as<ret>;
        };
    } else {
        using arg2 = fn_arg_t<substituted_func, 1>;
        return requires(arg1 lhs, arg2 rhs) {
            { do_binary_op<tag_op>(lhs, rhs) } -> std::same_as<ret>;
        };
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

consteval std::string op_tag_to_string(std::meta::info tag) {
    const auto full_sig = template_arguments_of(tag)[1];
    const bool self_is_lhs = remove_cvref(substitute(^^fn_arg_t, {full_sig, std::meta::reflect_constant(0)})) == ^^self;

    const auto after_remove_self = detail::remove_arg(full_sig, ^^self);
    const bool is_unary = extract<std::size_t>(substitute(^^fn_arg_count_v, {remove_fn_qualifiers(after_remove_self)})) == 0;

    return std::string{"_rjk__"} + (is_unary ? "unary_" : (self_is_lhs ? "lhs_" : "rhs_"))
        + enum_to_string(extract<std::meta::operators>(template_arguments_of(tag)[0]));
}

// The result can be used as follows to create a fixed_string:
// typename [:fixed_t:] my_fixed_str{str};
consteval fixed_result op_tag_to_fixed_string(std::meta::info tag) {
    if (template_of(tag) != ^^has_op) {
        throw std::logic_error{"Must pass in has_op tag"};
    }

    return make_fixed_string(op_tag_to_string(tag));
}
}

#endif