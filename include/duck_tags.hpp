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

#include <functional>

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

// Can be plugged into rjk::policy.
template <typename T>
concept duck_tag = [] consteval {
    return parent_of(^^T) == ^^::rjk::tags;
}();

// Plugged into rjk::duck.
template <duck_tag... Tags>
struct policy{};

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
    unary,
    variadic
};

namespace detail {
struct sig_info {
    op_overload_kind kind{};
    fn_qualifiers qualifiers{};
    std::meta::info after_remove_self{};
    std::meta::info erased_ptr_type{};
};

consteval sig_info analyze_op_sig(std::meta::info full_sig, std::meta::operators op) {
    const auto self_count = std::invoke(
    extract<std::size_t(*)(std::meta::info)>(
        substitute(^^count_args_of_type, {remove_fn_qualifiers(full_sig)})), ^^self);
    const bool member_style = (self_count == 0);

    const auto after_remove_self = member_style
        ? remove_fn_qualifiers(full_sig)
        : detail::remove_arg(full_sig, ^^self);
    const auto qualifiers = member_style
        ? qualifiers_of(full_sig)
        : qualifiers_of_target(full_sig, ^^self);

    const auto kind = std::invoke([&] {
        if (op == op_parentheses || op == op_square_brackets) {
           return op_overload_kind::variadic;
        }
        if (extract<std::size_t>(substitute(^^fn_arg_count_v, {after_remove_self})) == 0) {
            return op_overload_kind::unary;
        }
        if (member_style || remove_cvref(
            substitute(^^fn_arg_t, {full_sig, std::meta::reflect_constant(0)}))
            == ^^self) {
            return op_overload_kind::binary_lhs;
        }
        return op_overload_kind::binary_rhs;
    });

    return {
        .kind = kind,
        .qualifiers = qualifiers,
        .after_remove_self = after_remove_self,
        .erased_ptr_type = static_cast<bool>(qualifiers & fn_qualifiers::is_const)
            ? ^^const void* : ^^void*
    };
}

consteval sig_info analyze_op_tag(std::meta::info op_tag) {
    return analyze_op_sig(template_arguments_of(op_tag)[1],
        extract<std::meta::operators>(template_arguments_of(op_tag)[0]));
}

// Normalizes the function signature by replacing duck_t with the provided duck_type.
consteval std::meta::info normalized_sig(std::meta::info after_remove_self, std::meta::info duck_type) {
    return remove_noexcept(remove_fn_qualifiers(
        detail::substitute_fn_args(after_remove_self, ^^duck_t, duck_type)));
}
}

template <std::meta::operators Op, duck_tag... Tags>
consteval bool has_operator_tag(op_overload_kind kind = op_overload_kind::any_kind) {
    template for (constexpr auto tag : {^^Tags...}) {
        if constexpr (template_of(tag) == ^^has_op) {
            if constexpr ([:template_arguments_of(tag)[0]:] != Op) {
                continue;
            } else {
                if (kind == op_overload_kind::any_kind) {
                    return true;
                }

                constexpr static auto op_kind = detail::analyze_op_tag(tag).kind;

                if (kind == op_kind) {
                    return true;
                }
            }
        }
    }
    return false;
}

template <typename Type, typename DuckType, std::meta::info Tag>
consteval bool satisfies_op_tag() {
    constexpr static auto tag_op = [: template_arguments_of(Tag)[0] :];

    constexpr static auto [op_kind, qualifiers, after_remove_self, _]
        = detail::analyze_op_tag(Tag);

    using obj_type = std::conditional_t<
        static_cast<bool>(qualifiers & detail::fn_qualifiers::is_const), const Type, Type>;
    using ref_type = std::conditional_t<
        static_cast<bool>(qualifiers & detail::fn_qualifiers::rvalue_ref), obj_type&&, obj_type&>;

    // Special cases: operator() / operator[] can have more than two arguments
    if constexpr (tag_op == op_parentheses) {
        return extract<bool>(substitute(^^detail::callable_like_func_v,
            {^^Type, ^^ref_type, detail::normalized_sig(after_remove_self, ^^DuckType)}));
    }
    else if constexpr (tag_op == op_square_brackets) {
        return extract<bool>(substitute(^^detail::indexable_like_func_v,
            {^^Type, ^^ref_type, detail::normalized_sig(after_remove_self, ^^DuckType)}));
    }
    else if constexpr (op_kind == op_overload_kind::unary) {
        using ret = [: substitute(^^fn_return_type_t, {after_remove_self}) :];
        return requires(obj_type obj) {
            { do_unary_op<tag_op>(static_cast<ref_type>(obj)) } -> std::same_as<ret>;
        };
    } else {
        using sig  = [: detail::normalized_sig(after_remove_self, ^^DuckType) :];
        using ret  = fn_return_type_t<sig>;
        using arg1 = fn_arg_t<sig, 0>;
        if constexpr (op_kind == op_overload_kind::binary_lhs) {
            return requires(obj_type obj, arg1 rhs) {
                { do_binary_op<tag_op>(static_cast<ref_type>(obj), rhs) } -> std::same_as<ret>;
            };
        } else {
            return requires(arg1 lhs, obj_type obj) {
                { do_binary_op<tag_op>(lhs, static_cast<ref_type>(obj)) } -> std::same_as<ret>;
            };
        }
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
    const auto [op_kind, _, after_remove_self, _]
        = detail::analyze_op_tag(tag);

    const auto kind_identifier = std::invoke([&] -> std::string_view {
        switch (op_kind) {
            using enum op_overload_kind;
        case variadic:
            return "";
        case unary:
            return "unary_";
        case binary_lhs:
            return "lhs_";
        case binary_rhs:
            return "rhs_";
        default:
            throw std::logic_error{"unknown overload kind"};
        }
    });

    return std::string{"_rjk__"} + kind_identifier
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