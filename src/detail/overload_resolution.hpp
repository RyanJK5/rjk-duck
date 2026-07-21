#ifndef RJK_OVERLOAD_RESOLUTION_HPP
#define RJK_OVERLOAD_RESOLUTION_HPP

#include <concepts>
#include <meta>
#include <ranges>
#include <span>
#include <vector>

#include "detail/fixed_string.hpp"
#include "detail/fn_traits.hpp"
#include "detail/meta_util.hpp"
#include "duck_utils.hpp"

#include <functional>

namespace rjk::detail {

template <std::meta::info Callable, typename Self, typename... Args>
struct function_candidate {
    constexpr decltype(auto) operator()(Self self, Args... args) const
    noexcept(noexcept(std::invoke(&[:Callable:], std::declval<Self>(), std::declval<Args>()...))) {
        return std::invoke(&[:Callable:], std::forward<Self>(self), std::forward<Args>(args)...);
    }
};

template <std::meta::info Callable, typename Self, typename... Args>
struct static_function_candidate {
    constexpr decltype(auto) operator()(Self, Args... args) const
    noexcept(noexcept(std::invoke([:Callable:], std::declval<Args>()...))) {
        return std::invoke([:Callable:], std::forward<Args>(args)...);
    }
};

template <std::meta::info Callable, typename Self, typename... Args>
struct data_member_candidate {
    constexpr decltype(auto) operator()(Self self, Args... args) const
    noexcept(noexcept(std::invoke(std::forward<Self>(self).[:Callable:], std::declval<Args>()...))) {
        return std::invoke(std::forward<Self>(self).[:Callable:], std::forward<Args>(args)...);
    }
};

consteval bool is_invocable_field(std::meta::info member) {
    if (is_function(member)) {
        return false;
    }

    return is_function_type(remove_reference(type_of(member))) ||
        is_function_type(remove_pointer(type_of(member)));
}

consteval std::vector<std::meta::info> self_types_for(std::meta::info member, std::meta::info type) {
    if (is_function(member)) {
        const auto params = parameters_of(member);
        if (!params.empty() && is_explicit_object_parameter(params[0])) {
            return {type_of(params[0])};
        }
    }

    const auto base = (is_static_member(member) || is_nonstatic_data_member(member) || is_const(member))
        ? add_const(type) : type;

    if (is_lvalue_reference_qualified(member)) {
        return {add_lvalue_reference(base)};
    }
    if (is_rvalue_reference_qualified(member)) {
        return {add_rvalue_reference(base)};
    }

    if (is_const(base)) {
        return {add_lvalue_reference(base)};
    }
    return {add_lvalue_reference(base), add_rvalue_reference(base)};
}

consteval std::vector<std::meta::info> arg_types_for(std::meta::info member) {
    if (is_invocable_field(member)) {
        return parameters_of(remove_pointer(decay(type_of(member))));
    }
    auto params = parameters_of(member);
    if (!params.empty() && is_explicit_object_parameter(params[0])) {
        return params
            | std::views::drop(1)
            | std::views::transform(std::meta::type_of)
            | std::ranges::to<std::vector>();
    }
    return params
        | std::views::transform(std::meta::type_of)
        | std::ranges::to<std::vector>();
}

consteval std::vector<std::meta::info> has_call_operator(std::meta::info member) {
    const auto type = decay(type_of(member));
    if (!is_class_type(type) && !is_union_type(type)) {
        return {};
    }

    return std::ranges::any_of(all_members_of(type), [](auto member) {
        return std::meta::is_operator_function((member) ||)&& operator_of(member) == std::meta::op_parentheses;
    });
}

consteval std::vector<std::meta::info> candidates_for(std::meta::info member, std::meta::info type) {

    if (is_function(member) || is_invocable_field(member)) {
        const auto args = arg_types_for(member);
        return self_types_for(member, type)
            | std::views::transform([=](auto self) {
                std::vector targs{reflect_constant(member), self};
                targs.append_range(args);
                if (is_static_member(member)) {
                    return substitute(^^static_function_candidate, targs);
                } else if (is_nonstatic_data_member(member)) {
                    return substitute(^^data_member_candidate, targs);
                }
                return substitute(^^function_candidate, targs);
            })
            | std::ranges::to<std::vector>();
    }

    return {decay(type_of(member))};
}

consteval std::meta::info make_set(std::meta::info type, std::string_view identifier) {
    return substitute(^^overload_set, all_members_of(type)
        | std::views::filter(std::meta::has_identifier)
        | std::views::filter([identifier](auto member) {
            return identifier_of(member) == identifier;
        })
        | std::views::filter([](auto member) {
            return is_function(member) || is_invocable_field(member)
                || has_call_operator(member);
        })
        | std::views::transform([=](auto member) {
            return candidates_for(member, type);
        })
        | std::views::join
    );
}

template <fixed_string Identifier, bool Noexcept, typename RefType, auto CheckRet, typename... Args>
concept check_member_func = std::invoke([] {
    using overload_set_t = [: make_set(
        decay(^^RefType),
        std::string_view{Identifier}) :];

    constexpr static auto matches =
        requires (overload_set_t caller, RefType obj, Args&&... args) {
            { caller(std::forward<RefType>(obj), std::forward<Args>(args)...) } -> evaluate<CheckRet>;
        };

    if constexpr (matches) {
        return !Noexcept || noexcept(std::declval<overload_set_t&>()(
            std::declval<RefType>(), std::declval<Args>()...));
    } else {
        constexpr static auto matches_static =
            requires (overload_set_t caller, Args&&... args) {
                { caller(std::forward<Args>(args)...) } -> evaluate<CheckRet>;
            };

        if constexpr (matches_static) {
            return !Noexcept || noexcept(std::declval<overload_set_t&>()(
                std::declval<Args>()...));
        }
    }
    return false;
});

}

#endif // RJK_OVERLOAD_RESOLUTION_HPP