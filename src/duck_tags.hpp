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
#include "detail/overload_resolution.hpp"
#include "detail/vtable_generator.hpp"

#include <functional>
#include <ios>
#include <meta>
#include <meta>
#include <meta>
#include <meta>
#include <meta>
#include <meta>
#include <meta>
#include <meta>
#include <meta>
#include <ranges>

namespace rjk::detail {
consteval std::vector<std::meta::info> all_trait_members(std::meta::info trait);

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

template <typename T>
concept duck_type = (is_duck_type(^^T));

consteval static bool is_duck_container(std::meta::info type) {
    type = dealias(decay(type));
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
    std::meta::info trait_ret) {

    if (ret == trait_ret) {
        return true;
    }
    if (!is_duck_type(trait_ret) && template_of(trait_ret) != ^^duck_ptr) {
        return false;
    }

    const auto decayed_ret = decay(remove_pointer(decay(ret)));

    const auto meets_interface = [&] {
        return std::ranges::all_of(template_arguments_of(trait_ret), [&](auto trait) {
            return satisfies_trait(decayed_ret, trait);
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
            return meets_interface();
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
            return meets_interface();
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
            return meets_interface();
        }
        return true;
    }
    return false;
}

template <typename T, typename TraitRet>
consteval bool check_return_value(std::meta::info ret) {
    return detail::is_return_compatible(ret, ^^T, ^^TraitRet);
}

consteval bool is_strictly_compatible(std::meta::info member, std::meta::info sig,
    std::meta::info test_type) {

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
        ret, test_type, trait_ret);
    if (same_returns && is_noexcept(sig) &&
        !is_conversion_noexcept(trait_ret, ret)) {
        return false;
        }

    const auto same_noexcept = !is_noexcept(sig) || is_noexcept(member);

    return same_params && same_qualifiers && same_returns && same_noexcept;
}

consteval bool is_compatible_sig_in_impl(std::meta::info member, std::meta::info sig,
    std::meta::info test_type) {
    const auto same_params = std::ranges::equal(
        parameters_of(member) | std::views::drop(1)
        | std::views::transform(std::meta::type_of)
        | std::views::transform(std::meta::dealias),
        parameters_of(sig)
    );

    auto func_qualifiers = detail::qualifiers_of(sig);

    const auto impl_qualifiers = detail::qualifiers_of_target(type_of(member), test_type);
    const auto same_qualifiers = std::invoke([=] {
        if (has_template_arguments(member) && impl_qualifiers == detail::fn_qualifiers::rvalue_ref) { // perfect forwarding
            // For const methods, you must specifically use const & / const &&.
            return func_qualifiers == detail::fn_qualifiers::none;
        }
        if (impl_qualifiers == detail::fn_qualifiers::is_const) {
            return static_cast<bool>(func_qualifiers & detail::fn_qualifiers::is_const);
        }

        return impl_qualifiers == func_qualifiers;
    });

    const auto same_returns = detail::is_return_compatible(
        dealias(return_type_of(member)), test_type,
        dealias(return_type_of(sig)));

    const auto same_noexcept = !is_noexcept(sig) || is_noexcept(member);

    return same_params && same_qualifiers && same_returns && same_noexcept;
}

consteval std::optional<std::meta::info> find_impl_specialization(
    std::meta::info type, std::meta::info trait, std::string_view member_name,
    std::meta::info full_sig) {
    const auto bases = family_tree_for(trait);
    for (const auto base : bases) {
        const auto impl_struct = substitute(^^impl, {type, remove_const(base)});
        const auto members = members_of(impl_struct, std::meta::access_context::unprivileged());
        const auto member = std::ranges::find_if(members, [=](auto m) {
            if (!is_static_member(m)) {
                return false;
            }
            if (!has_identifier(m)) {
                return false;
            }
            if (identifier_of(m) != member_name) {
                return false;
            }
            if (is_function(m)) {
                return is_compatible_sig_in_impl(
                    m, full_sig, type);
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
                    full_sig, type);
            }
            return false;
        });

        if (member != members.end()) {
            return substitute(^^call_wrapper, {reflect_constant(*member)});
        }
    }
    return std::nullopt;
}

template <typename Trait>
constexpr inline auto function_lookup_rule_for = std::invoke([] {
    constexpr static auto has_lookup_rule = requires(Trait t) {
        { t.function_lookup } -> std::convertible_to<lookup_rule>;
    };
    if constexpr (has_lookup_rule) {
        return Trait{}.function_lookup;
    } else {
        return lookup_rule::none;
    }
});

consteval bool has_member(const member_info& info, std::meta::info type, std::meta::info sig,
    lookup_rule rule) {
    if (rule == lookup_rule::strict) {
        return std::ranges::any_of(
            all_members_of(type)
            | std::views::filter(std::meta::is_function)
            | std::views::filter(std::meta::has_identifier)
            | std::views::filter([=](auto member) {
                return identifier_of(member) == std::string_view{info.identifier};
            }),
            [=](auto member) {
                return is_strictly_compatible(member, sig, type);
            });
    }

    const auto check_ret = [=](auto ret) {
        const auto same_returns = is_return_compatible(ret,
            type, return_type_of(sig));

        const auto trait_ret = dealias(return_type_of(sig));
        if (same_returns && is_noexcept(sig) &&
            !is_conversion_noexcept(trait_ret, ret)) {
            return false;
            }

        return same_returns;
    };

    return std::ranges::all_of(
        detail::self_types_for(sig, type),
        [=](auto self) {
            std::vector args{
                std::meta::reflect_constant(info),
                std::meta::reflect_constant(is_noexcept(sig)),
                self,
                std::meta::reflect_constant(check_ret)
            };
            args.append_range(parameters_of(sig));

            return extract<bool>(substitute(^^detail::check_member_func, args));
        }
    );
}

consteval bool matches_function(std::meta::info type, std::meta::info trait, std::meta::info member) {
    if (!is_class_type(type) && !is_union_type(type)) {
        return false;
    }

    const member_info info{
        .identifier = fixed_string{identifier_of(member)},
        .param_count = parameters_of(member).size()
    };

    const auto rule = extract<lookup_rule>(substitute(
        ^^function_lookup_rule_for, {trait}));
    const bool has_interface = has_member(
        info, type, type_of(member), rule);

    if (has_interface) {
        return true;
    } else {
        const auto specialization = detail::find_impl_specialization(
            type, trait, identifier_of(member), type_of(member));
        if (specialization.has_value()) {
            return true;
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

consteval op_overload_kind op_kind_of(std::meta::info op_func) {
    const auto params = parameters_of(op_func);
    const auto op = operator_of(op_func);
    if (op == op_parentheses || op == op_square_brackets) {
        return op_overload_kind::variadic;
    }
    if (params.size() == 0uz) {
        return op_overload_kind::unary;
    }
    if (detail::has_annotation(op_func, ^^right_side)) {
        return op_overload_kind::binary_rhs;
    }
    return op_overload_kind::binary_lhs;
}

consteval std::vector<std::meta::info> all_trait_members(std::meta::info trait) {
    trait = dealias(trait);

    if (extract<bool>(substitute(^^is_perf_option, {trait}))) {
        return {};
    }

    const auto using_like = extract<bool>(substitute(^^is_like, {trait}));

    const auto subject = using_like ? template_arguments_of(trait)[0] : trait;

    constexpr static auto ctx = std::meta::access_context::unprivileged();

    auto starting_list = using_like ? all_members_of(subject) : members_of(subject, ctx);
    auto ret = starting_list
        | std::views::filter([=](auto member) {
            if (!is_user_declared(member) && !is_user_declared(member)) {
                return false;
            }
            if (is_copy_constructor(member) && is_defaulted(member)) {
                return true;
            }
            if (is_function(member) && has_identifier(member)) {
                return true;
            }
            if (is_operator_function(member)) {
                return true;
            }
            if ((is_nonstatic_data_member(member) || is_static_member(member)) && (type_of(member) == ^^lookup_rule)) {
                return identifier_of(member) == "function_lookup";
            }
            if (!using_like) {
                display_error(std::string{"Trait '"} + display_string_of(trait)
                    + "' cannot hold non-member function '" + display_string_of(member) + "'");
            }
            return false;
        })
        | std::views::filter([=](auto member) {
            if (is_const(trait)) {
                return is_const(member);
            }
            return true;
        })
        | std::views::filter([=](auto member) {
            if (!using_like) {
                return true;
            }
            return std::invoke(extract<bool(*)(std::meta::info)>(
                template_arguments_of(trait)[1]), member);
        })
        | std::ranges::to<std::vector>();

    if (!using_like) {
        auto base_members = bases_of(trait, ctx)
            | std::views::transform([trait](auto base) {
                const auto base_type = type_of(base);
                if (is_const(trait)) {
                    return add_const(base_type);
                }
                return base_type;
            })
            | std::views::transform(all_trait_members)
            | std::views::join;
        ret.append_range(base_members);
    }

    return ret;
}

template <typename Trait>
constexpr inline auto members_for = define_static_array(all_trait_members(^^Trait));

consteval bool matches_operator(std::meta::info type, std::meta::info op_member) {
    const auto member_op = operator_of(op_member);

    const auto obj_type = is_const(op_member) ? add_const(type) : type;
    const auto ref_type = is_rvalue_reference_qualified(op_member)
        ? add_rvalue_reference(op_member) : add_lvalue_reference(op_member);
    const auto params = parameters_of(op_member);

    const auto member_noexcept = is_noexcept(op_member);

    // Special cases: operator() / operator[] can have more than two arguments
    // TODO: These do not correctly handle noexcept
    if (member_op == op_parentheses) {
        if (!is_invocable_type(ref_type, params)) {
            return false;
        }
        const auto ret_type = invoke_result(ref_type, parameters_of(op_member));
        const auto has_member = detail::is_return_compatible(ret_type,
            type, return_type_of(op_member));
        return has_member;
    }
    if (member_op == op_square_brackets) {
        if (!detail::is_subscriptable(ref_type, params)) {
            return false;
        }
        const auto ret_type = detail::subscript_result(ref_type, parameters_of(op_member));
        const auto has_member = detail::is_return_compatible(ret_type,
            type, return_type_of(op_member));
        return has_member;
    }

    const auto check_ret = substitute(^^check_return_value, {type, return_type_of(op_member)});

    if (params.size() == 0) {
        const bool has_unary = extract<bool(*)()>(substitute(^^detail::check_unary_op, {
            reflect_constant(member_op), reflect_constant(member_noexcept),
            ref_type, check_ret
        }))();
        return has_unary;
    }

    const auto arg1 = type_of(params[0]);
    if (!detail::has_annotation(op_member, ^^right_side)) {
        const bool has_binary_lhs = extract<bool(*)()>(substitute(^^detail::check_binary_op, {
            reflect_constant(member_op), reflect_constant(member_noexcept),
            ref_type, arg1, check_ret
        }))();
        return has_binary_lhs;
    } else {
        const bool has_binary_rhs = extract<bool(*)()>(substitute(^^detail::check_binary_op, {
            reflect_constant(member_op), reflect_constant(member_noexcept),
            ref_type, arg1, check_ret
        }))();
        return has_binary_rhs;
    }
}

consteval bool satisfies_trait(std::meta::info type, std::meta::info trait,
    const std::meta::reflection_range auto& members) {
    return std::ranges::all_of(members, [=](auto member) {
        if (is_copy_constructor(member)) {
            return is_copy_constructible_type(type);
        }
        if (is_operator_function(member)) {
           return matches_operator(type, member);
        }
        return matches_function(type, trait, member);
    });
}

consteval bool satisfies_trait(std::meta::info type, std::meta::info trait) {
    return satisfies_trait(type, trait, all_trait_members(trait));
}

// Explicit Trait1 to prevent using satisfies for zero traits
template <typename T, typename Trait1, typename... Traits>
concept satisfies = std::invoke([] consteval {
    std::vector<std::meta::info> traits{^^Trait1, ^^Traits...};
    return std::ranges::all_of(traits, [](auto trait) {
        return satisfies_trait(^^T, trait);
    });
});

consteval std::string operator_to_string(std::meta::info member) {
    const auto kind_identifier = std::invoke([=] -> std::string_view {
        switch (op_kind_of(member)) {
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

    return std::string{"_rjk_"} + kind_identifier + enum_to_string(operator_of(member));
}

}
#endif