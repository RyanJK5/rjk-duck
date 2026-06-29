// clang-format off

#ifndef RJK_DUCK_TAGS_HPP
#define RJK_DUCK_TAGS_HPP

#include "generated/do_unary_op.hpp"
#include "generated/do_binary_op.hpp"
#include "detail/fixed_string.hpp"
#include "detail/flag_enum.hpp"
#include "detail/fn_traits.hpp"
#include "detail/remove_fn_qualifiers.hpp"
#include "detail/display_error.hpp"
#include "detail/meta_util.hpp"

#include <functional>
#include <ranges>

namespace rjk {

template <typename Type, typename RelevantTrait, bool PrettyError, typename... Tags>
consteval bool satisfies_tags();

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

template <fixed_string Identifier, function_signature Func>
struct has_fn {};

template <std::meta::operators Operator, function_signature Func>
struct has_op {};

struct copy_tag{};

// TODO: Remove when GCC fixes bug
template <fixed_string Identifier, std::meta::info Func>
using has_fn_meta = has_fn<Identifier, typename [:Func:]>;

// TODO: Remove when GCC fixes bug
template <std::meta::operators Operator, std::meta::info Func>
using has_op_meta = has_op<Operator, typename [:Func:]>;

// Used for denoting the relative location of two ducks in a has_op signature.
struct self{};

// Can be plugged into rjk::policy.
template <typename T>
concept duck_tag = std::same_as<T, copy_tag> || (has_template_arguments(^^T) && (
    template_of(^^T) == ^^has_fn ||
    template_of(^^T) == ^^has_op));

// Plugged into rjk::duck.
template <duck_tag... Tags>
struct policy{};

template <typename Func>
concept is_meta_predicate = std::invocable<Func, std::meta::info> &&
    std::same_as<std::invoke_result_t<Func, std::meta::info>, bool>;

// Models a trait based on the type's public interface and the provided predicate.
template <typename T, is_meta_predicate auto Predicate =
    [] (std::meta::info) consteval { return true; }>
struct like {};

template <is_meta_predicate auto... Predicates>
constexpr inline auto all_of = [](std::meta::info member) {
    return (std::invoke(Predicates, member) && ...);
};

template <is_meta_predicate auto... Predicates>
constexpr inline auto any_of = [](std::meta::info member) {
    return (std::invoke(Predicates, member) || ...);
};

template <is_meta_predicate auto... Predicates>
constexpr inline auto none_of = [](std::meta::info member) {
    return !(std::invoke(Predicates, member) || ...);
};

template <fixed_string... Whitelist>
constexpr inline auto include = [](std::meta::info member) {
    if (!has_identifier(member)) {
        return false;
    }
    const auto whitelist = std::array<fixed_string, sizeof...(Whitelist)>{Whitelist...}
        | std::views::transform([](auto str) {
            return std::string_view{str};
        });
    return std::ranges::contains(whitelist, identifier_of(member));
};

template <fixed_string... Blacklist>
constexpr inline auto exclude = [](std::meta::info member) {
    if (!has_identifier(member)) {
        return true;
    }

    const auto blacklist = std::array<fixed_string, sizeof...(Blacklist)>{Blacklist...}
        | std::views::transform([](auto str) {
            return std::string_view{str};
        });
    return !std::ranges::contains(blacklist, identifier_of(member));
};

// Passed as a policy to rjk::duck to allow copying.
using copyable = policy<copy_tag>;

// The following are meant to be used as attributes.

// [[=rjk::trait]] specifies that a struct will be used as a trait.
constexpr inline struct{} trait{};

// [[=rjk::right_side]] specifies that an operator function is being defined with self as the last argument.
constexpr inline struct{} right_side{};

// [[=rjk::both_sides]] specifies that an operator function needs both self + T and T + self.
constexpr inline struct{} both_sides{};

// [[=rjk::perf_options]] specifies a trait that changes the default performance options for
// rjk::duck. Currently, these means customizing sbo_size and sbo_alignment.
constexpr inline struct{} perf_options{};

template <typename T>
concept is_policy = (has_template_arguments(^^T) && template_of(^^T) == ^^policy);

template <typename T>
concept is_like = (has_template_arguments(^^T) && template_of(^^T) == ^^like);

template <typename T>
concept is_perf_option = detail::has_annotation(^^T, ^^perf_options);

template <typename T>
concept is_trait = (
    is_policy<T> || is_like<T> || is_perf_option<T> ||
    detail::has_annotation(^^T, ^^trait));

template <is_trait... Traits>
class duck;

template <is_trait... Traits>
class duck_view;

template <is_trait... Traits>
class duck_ptr;

// Allows users to add an additional
template <typename T, is_trait Trait>
struct impl {};

namespace detail {
consteval std::string format_func_name(auto name, std::meta::info signature) {
    const auto disp_str = display_string_of(signature);
    return std::string{disp_str.substr(0, disp_str.find('('))}
        + " " + name + disp_str.substr(disp_str.find('('));
}

consteval std::vector<std::meta::info> members_to_tags(std::meta::info trait);

// NOTE: duck_ptr is not a duck type, it's just a wrapper around duck_view.
consteval static bool is_duck_type(std::meta::info type) {
    type = dealias(decay(type));

    if (!has_template_arguments(type)) {
        return false;
    }
    if (template_of(type) != ^^duck && template_of(type) != ^^duck_view) {
        return false;
    }
    return true;
}

consteval static bool is_duck_container(std::meta::info type) {
    return has_template_arguments(type)
        && is_type(type)
        && template_of(type) == ^^duck;
}

template <typename TraitRet, typename ActualRet>
consteval bool is_conversion_noexcept_impl() {
    if constexpr (is_duck_container(^^TraitRet)) {
        return TraitRet::template nothrow_constructor<std::decay_t<ActualRet>, ActualRet>;
    } else {
        return true;
    }
};

consteval bool is_conversion_noexcept(std::meta::info trait_ret, std::meta::info actual_ret) {
    return std::invoke(extract<bool(*)()>(substitute(^^is_conversion_noexcept_impl, {trait_ret, actual_ret})));
}

consteval bool is_return_compatible(std::meta::info ret,
    std::meta::info tested_type,
    std::meta::info trait_ret,
    bool pretty_error) {

    if (ret == trait_ret) {
        return true;
    }
    if (!has_template_arguments(trait_ret)) {
        return false;
    }
    if (!detail::is_duck_type(trait_ret) && template_of(trait_ret) != ^^duck_ptr) {
        return false;
    }

    const auto args = template_arguments_of(trait_ret)
        | std::views::transform(members_to_tags)
        | std::views::join
        | std::ranges::to<std::vector>();
    const auto decayed_ret = decay(remove_pointer(decay(ret)));

    const auto meets_tags = [&] {
        return std::ranges::all_of(template_arguments_of(trait_ret), [&](auto trait) {
            const auto sub_args = std::views::concat(
                std::array{decayed_ret, trait, std::meta::reflect_constant(pretty_error)},
                members_to_tags(trait)
            );
            const auto meets_trait = std::invoke(
                extract<bool(*)()>(substitute(^^satisfies_tags, sub_args)));
            return meets_trait;
        });
    };

    if (template_of(trait_ret) == ^^duck_view) {
        if (!is_lvalue_reference_type(ret)) {
            return false;
        }

        const bool all_const = std::ranges::all_of(
            template_arguments_of(trait_ret), std::meta::is_const);

        if (!all_const && is_const(remove_reference(ret))) {
            return false;
        }

        if (decay(tested_type) != decayed_ret) {
            return meets_tags();
        }
        return true;
    }
    if (template_of(trait_ret) == ^^duck_ptr) {
        if (!is_pointer_type(ret)) {
            return false;
        }

        const bool all_const = std::ranges::all_of(
            template_arguments_of(trait_ret), std::meta::is_const);

        if (!all_const && is_const(remove_reference(ret))) {
            return false;
        }

        if (decay(tested_type) != decayed_ret) {
            return meets_tags();
        }
        return true;
    }
    if (template_of(trait_ret) == ^^duck) {
        if (is_lvalue_reference_type(ret)) {
            return false;
        }
        if (is_rvalue_reference_type(ret) && is_const(remove_reference(ret))) {
            return false;
        }

        if (decay(tested_type) != decayed_ret) {
            return meets_tags();
        }
        return true;
    }
    return false;
}

consteval bool is_compatible_sig_in_impl(std::meta::info member, std::meta::info sig,
    std::meta::info test_type, bool pretty_error) {
    const auto same_params = std::ranges::equal(
        parameters_of(member) | std::views::drop(1)
        | std::views::transform(std::meta::type_of)
        | std::views::transform(std::meta::dealias),
        parameters_of(sig)
    );

    auto func_qualifiers = detail::qualifiers_of(sig);
    if (func_qualifiers == detail::fn_qualifiers::none) {
        // If there are no qualifiers at all, the function is valid for both
        // non-const lvalue and rvalues. When matching against the function
        // signature in rjk::impl, we want to look for the forwarding reference.
        func_qualifiers |= detail::fn_qualifiers::rvalue_ref;
    }

    const auto same_qualifiers = detail::qualifiers_of_target(type_of(member), test_type) == func_qualifiers;

    const auto same_returns = detail::is_return_compatible(
        dealias(return_type_of(member)), test_type,
        dealias(return_type_of(sig)), pretty_error);

    const auto same_noexcept = !is_noexcept(sig) || is_noexcept(member);

    return same_params && same_qualifiers && same_returns && same_noexcept;
}

consteval bool is_compatible_sig(std::meta::info member, std::meta::info sig,
    std::meta::info test_type, bool pretty_error) {

    const auto same_params =std::ranges::equal(
        parameters_of(member)
        | std::views::transform(std::meta::type_of)
        | std::views::transform(std::meta::dealias),
        parameters_of(sig)
    );

    const auto same_qualifiers = detail::qualifiers_of(member) == detail::qualifiers_of(sig);

    const auto ret = dealias(return_type_of(member));
    const auto trait_ret = dealias(return_type_of(sig));
    const auto same_returns = detail::is_return_compatible(
        ret, test_type, trait_ret, pretty_error);
    if (same_returns && is_noexcept(sig) &&
        !is_conversion_noexcept(trait_ret, ret)) {
        return false;
    }

    const auto same_noexcept = !is_noexcept(sig) || is_noexcept(member);

    return same_params && same_qualifiers && same_returns && same_noexcept;
}

consteval std::optional<std::meta::info> find_impl_specialization(
    std::meta::info type, std::meta::info trait, std::string_view member_name,
    std::meta::info full_sig, bool pretty_error) {
    const auto bases = family_tree_for(trait);
    for (const auto base : bases) {
        const auto impl_struct = substitute(^^impl, {type, remove_const(base)});
        const auto members = members_of(impl_struct, std::meta::access_context::unprivileged());
        const auto member = std::ranges::find_if(members, [=](auto m) {
            if (!has_identifier(m)) {
                return false;
            }
            if (identifier_of(m) != member_name) {
                return false;
            }
            if (is_function(m)) {
                return is_compatible_sig_in_impl(
                    m, full_sig, type, pretty_error);
            }
            if (is_function_template(m)) {
                if (!can_substitute(m, {type})) {
                    return false;
                }
                const auto func = substitute(m, {type});
                if (!is_function(func)) {
                    return false;
                }
                return is_compatible_sig_in_impl(func,
                    full_sig, type, pretty_error);
            }
            return false;
        });

        if (member != members.end()) {
            return *member;
        }
    }
    return std::nullopt;
}
}

template <typename Type, typename RelevantTrait, std::meta::info Tag, bool PrettyErrors>
consteval bool satisfies_fn_tag() {
    static_assert(is_class_type(^^Type));

    constexpr static auto type_members = define_static_array(detail::all_members_of(^^Type));
    constexpr static auto name = std::string_view{[:template_arguments_of(Tag)[0]:]};
    constexpr static auto sig = template_arguments_of(Tag)[1];

    constexpr static bool meets_tag = std::ranges::any_of(type_members, [](auto member) {
        return has_identifier(member) &&
            identifier_of(member) == name &&
            is_function(member) && detail::is_compatible_sig(member, sig, ^^Type, PrettyErrors);
    });

    if constexpr (meets_tag) {
        return true;
    } else {
        const auto specialization = detail::find_impl_specialization(
            ^^Type, ^^RelevantTrait, name, sig, PrettyErrors);
        if (specialization.has_value()) {
            return true;
        }

        if constexpr (PrettyErrors) {
            const auto error_message = std::string{display_string_of(^^Type)}
                + " does not define member function '"
                + detail::format_func_name(name, sig) + "'"
                + " and does not specialize rjk::impl";

            detail::display_error(error_message);
        }
        return false;
    }
}

enum struct op_overload_kind {
    any_kind,
    binary_lhs,
    binary_rhs,
    unary,
    variadic
};

namespace detail {

template <typename T>
struct init_tag{};

struct sig_info {
    op_overload_kind kind{};
    fn_qualifiers qualifiers{};
    std::meta::info after_remove_self{};
    std::meta::info erased_ptr_type{};
};

consteval sig_info analyze_op_sig(std::meta::info full_sig, std::meta::operators op) {
    const auto self_count = std::ranges::count(
        parameters_of(full_sig) | std::views::transform(std::meta::decay),
        ^^self);
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
        if (parameters_of(after_remove_self).size() == 0) {
            return op_overload_kind::unary;
        }
        if (member_style || remove_cvref(parameters_of(full_sig)[0]) == ^^self) {
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

consteval bool is_const_tag(std::meta::info tag) {
    if (tag == ^^copy_tag) {
        return true;
    } else if (template_of(tag) == ^^has_fn) {
        return static_cast<bool>(qualifiers_of(template_arguments_of(tag)[1]) & fn_qualifiers::is_const);
    } else if (template_of(tag) == ^^has_op) {
        auto qualifiers = analyze_op_tag(tag).qualifiers;
        return static_cast<bool>(qualifiers & fn_qualifiers::is_const);
    } else {
        return true;
    }
}

consteval std::meta::info make_rhs_signature(std::meta::info member) {
    auto self_t = ^^self;
    if (is_const(member)) {
        self_t = add_const(self_t);
    }
    if (is_rvalue_reference_qualified(member)) {
        self_t = add_rvalue_reference(self_t);
    } else {
        self_t = add_lvalue_reference(self_t);
    }

    const auto base_func_t = remove_fn_qualifiers(type_of(member));
    const auto with_self = append_arg(self_t, base_func_t);

    return dealias(substitute(^^has_op_meta, {
        std::meta::reflect_constant(operator_of(member)),
        reflect_constant(with_self)
    }));
}

consteval std::vector<std::meta::info> members_to_tags(std::meta::info trait) {
    trait = dealias(trait);

    const auto constness_filter = [trait](auto tag) {
        if (!is_const(trait)) {
            return true;
        }
        return is_const_tag(tag);
    };

    if (extract<bool>(substitute(^^is_policy, {trait}))) {
        return template_arguments_of(trait)
            | std::views::filter(constness_filter)
            | std::ranges::to<std::vector>();
    }
    if (extract<bool>(substitute(^^is_perf_option, {trait}))) {
        if (has_annotation(trait, ^^::rjk::trait)) {
            display_error(std::string{display_string_of(trait)} +
                " cannot use both [[=rjk::perf_options]] and [[=rjk::trait]].");
        }
        return {};
    }

    const auto using_like = extract<bool>(substitute(^^is_like, {trait}));

    const auto subject = using_like ? template_arguments_of(trait)[0] : trait;

    constexpr static auto ctx = std::meta::access_context::unprivileged();

    auto starting_list = using_like ? all_members_of(subject) : members_of(subject, ctx);
    auto trait_tags = starting_list
        | std::views::filter([=](auto member) {
            if (is_function(member) && !is_user_declared(member)) {
                return false;
            }
            if (is_function(member) && has_identifier(member)) {
                return true;
            }
            if (is_operator_function(member)) {
                return true;
            }
            if (!using_like) {
                display_error(std::string{"Trait '"} + display_string_of(trait)
                    + "' cannot hold non-member function '" + display_string_of(member) + "'");
            }
            return false;
        })
        | std::views::filter([=](auto member) {
            if (!using_like) {
                return true;
            }
            return std::invoke(extract<bool(*)(std::meta::info)>(
                template_arguments_of(trait)[1]), member);
        })
        | std::views::transform([](auto member) -> std::vector<std::meta::info> {
            if (is_operator_function(member)) {
                if (has_annotation(member, ^^right_side)) {
                    return {make_rhs_signature(member)};
                }

                const auto lhs_sig = dealias(substitute(^^has_op_meta, {
                    std::meta::reflect_constant(operator_of(member)),
                    reflect_constant(type_of(member))
                }));

                if (has_annotation(member, ^^both_sides)) {
                    return {lhs_sig, make_rhs_signature(member)};
                }

                return {lhs_sig};
            } else if (is_function(member)) {
                const fixed_string fixed_str{identifier_of(member)};
                return {dealias(substitute(^^has_fn_meta, {
                    std::meta::reflect_constant(fixed_str),
                    reflect_constant(type_of(member))}
                ))};
            } else {
                return {};
            }
        })
        | std::views::join
        | std::views::filter(constness_filter)
        | std::ranges::to<std::vector>();

    if (!using_like) {
        auto base_tags = bases_of(trait, ctx)
            | std::views::transform([trait](auto base) {
                const auto base_type = type_of(base);
                if (is_const(trait)) {
                    return add_const(base_type);
                }
                return base_type;
            })
            | std::views::transform(members_to_tags)
            | std::views::join;
        trait_tags.append_range(base_tags);
    }

    return trait_tags;
}

}

template <std::meta::operators Op, duck_tag... Tags>
consteval bool has_operator_tag(op_overload_kind kind = op_overload_kind::any_kind) {
    template for (constexpr auto tag : {^^Tags...}) {
        if constexpr ((tag != ^^copy_tag) && template_of(tag) == ^^has_op) {
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

template <typename Type, std::meta::info Tag, bool PrettyErrors>
consteval bool satisfies_op_tag() {
    constexpr static auto tag_op = [: template_arguments_of(Tag)[0] :];

    constexpr static auto [op_kind, qualifiers, after_remove_self, _]
        = detail::analyze_op_tag(Tag);

    using obj_type = std::conditional_t<
        static_cast<bool>(qualifiers & detail::fn_qualifiers::is_const), const Type, Type>;
    using ref_type = std::conditional_t<
        static_cast<bool>(qualifiers & detail::fn_qualifiers::rvalue_ref), obj_type&&, obj_type&>;

    constexpr static auto sig_refl = template_arguments_of(Tag)[1];

    constexpr static fixed_string pretty_error{
        std::string{display_string_of(^^Type)}
            + " does not define '"
            + detail::format_func_name(std::string{"operator"} +
                symbol_of(tag_op), sig_refl) + "' as member"
            + " or free function"};

    [[maybe_unused]] constexpr static auto check_ret = [](auto ret) {
        return detail::is_return_compatible(ret,
            ^^Type, return_type_of(sig_refl), PrettyErrors);
    };

    // Special cases: operator() / operator[] can have more than two arguments
    if constexpr (tag_op == op_parentheses || tag_op == op_square_brackets) {
        constexpr static bool has_member = std::ranges::any_of(
            detail::all_members_of(^^Type),
            [](auto member) {
                return is_operator_function(member) &&
                    operator_of(member) == tag_op &&
                    detail::is_compatible_sig(member, sig_refl, ^^Type, PrettyErrors);
            }
        );
        if constexpr (PrettyErrors) {
            static_assert(has_member, pretty_error);
        }
        return has_member;
    }
    else if constexpr (op_kind == op_overload_kind::unary) {
        constexpr static bool has_unary = detail::check_unary_op<
            tag_op, is_noexcept(sig_refl), obj_type, ref_type, check_ret>();
        if constexpr (PrettyErrors) {
            static_assert(has_unary, pretty_error);
        }
        return has_unary;
    } else {
        constexpr static auto sig = remove_fn_qualifiers(after_remove_self);
        using ret  = [: return_type_of(sig) :];
        using arg1 = [: parameters_of(sig)[0] :];
        if constexpr (op_kind == op_overload_kind::binary_lhs) {
            constexpr static bool has_binary_lhs = detail::check_binary_op<
                tag_op, is_noexcept(sig_refl), obj_type, ref_type, arg1, arg1, check_ret
            >();

            if constexpr (PrettyErrors) {
                static_assert(has_binary_lhs, pretty_error);
            }
            return has_binary_lhs;
        } else {
            constexpr static bool has_binary_rhs = detail::check_binary_op<
                tag_op, is_noexcept(sig_refl), arg1, arg1, obj_type, ref_type, check_ret
            >();

            if constexpr (PrettyErrors) {
                static_assert(has_binary_rhs, std::invoke([] consteval {
                    std::string error{display_string_of(dealias(^^arg1))};
                    error += " does not define '";

                    const auto dummy_func = detail::make_func(dealias(^^ret), {dealias(^^ref_type)});
                    const auto identifier = std::string{"operator"} + symbol_of(tag_op);
                    error += detail::format_func_name(identifier, dummy_func);
                    error += "' as member or free function";
                    return error;
                }));
            }
            return has_binary_rhs;
        }
    }
}
template <typename Type, typename RelevantTrait, bool PrettyErrors, typename... Tags>
consteval bool satisfies_tags() {
    if constexpr (has_template_arguments(^^Type) && (
        template_of(^^Type) == ^^std::in_place_type_t ||
        template_of(^^Type) == ^^detail::init_tag ||
        template_of(^^Type) == ^^duck ||
        template_of(^^Type) == ^^duck_view)) {
        return false; // Avoids static assertion triggering during subsumption
    } else {
        template for (constexpr auto tag : {^^Tags...}) {
            if constexpr (tag == ^^copy_tag) {
                if constexpr (std::copyable<Type>) {
                    continue;
                } else {
                    if constexpr (PrettyErrors) {
                        static_assert(false, std::string{display_string_of(^^Type)}
                            + " is not copyable");
                    }
                }
            }
            else if constexpr (template_of(tag) == ^^has_fn) {
                if (satisfies_fn_tag<Type, RelevantTrait, tag, PrettyErrors>()) {
                    continue;
                }
            }
            else if constexpr (template_of(tag) == ^^has_op) {
                if constexpr (satisfies_op_tag<Type, tag, PrettyErrors>()) {
                    continue;
                }
            }
            return false;
        }
        return true;
    }
}

// Explicit Trait1 to prevent using satisfies for zero traits
template <typename T, typename Trait1, typename... Traits>
concept satisfies = is_trait<Trait1> && (is_trait<Traits> && ...) && std::invoke([] consteval {
    const std::array traits{^^Trait1, ^^Traits...};
    return std::ranges::all_of(traits, [](auto trait) {
        const auto satisfy_func = substitute(^^satisfies_tags,
            std::views::concat(
                std::array{^^T, trait, std::meta::reflect_constant(false)},
                detail::members_to_tags(trait)
            ));
        return std::invoke(extract<bool(*)()>(satisfy_func));
    });
});

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
            return "";
        }
    });

    return std::string{"_rjk_"} + kind_identifier
        + enum_to_string(extract<std::meta::operators>(template_arguments_of(tag)[0]));
}

consteval fixed_string op_tag_to_fixed_string(std::meta::info tag) {
    return fixed_string{std::string_view{op_tag_to_string(tag)}};
}
}

#endif