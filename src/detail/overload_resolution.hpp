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
    noexcept(noexcept(std::declval<Self>().[:Callable:](std::declval<Args>()...))) {
        return std::forward<Self>(self).[:Callable:](std::forward<Args>(args)...);
    }
};

template <std::meta::info Callable, typename Self, typename... Args>
struct explict_obj_candidate {
    constexpr decltype(auto) operator()(Self self, Args... args) const
    noexcept(noexcept(std::invoke(&[:Callable:], std::declval<Self>(), std::declval<Args>()...))) {
        return std::invoke(&[:Callable:], std::forward<Self>(self), std::forward<Args>(args)...);
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

struct member_info {
    fixed_string identifier;
    std::size_t param_count;
};

consteval bool uses_explicit_object(const std::meta::reflection_range auto& params) {
    return !params.empty() && is_explicit_object_parameter(params[0]);
}

consteval bool uses_explicit_object(std::meta::info member) {
    return is_function(member) && uses_explicit_object(parameters_of(member));
}

consteval std::vector<std::meta::info> arg_types_for(std::meta::info member, const member_info& info) {
    if (is_invocable_field(member)) {
        return parameters_of(remove_pointer(decay(type_of(member))));
    }

    const auto fullParams = parameters_of(member);
    if (uses_explicit_object(fullParams)) {
        return fullParams
            | std::views::drop(1)
            | std::views::transform(std::meta::type_of)
            | std::ranges::to<std::vector>();
    }

    const auto endItr = [=, &fullParams] {
        if (fullParams.size() < info.param_count) {
            return fullParams.end();
        }

        return std::max(
            fullParams.begin() + static_cast<std::ptrdiff_t>(info.param_count),
            std::ranges::find_if(fullParams, std::meta::has_default_argument));
    }();
    const auto params = std::ranges::subrange(fullParams.begin(), endItr);

    return params
        | std::views::transform(std::meta::type_of)
        | std::ranges::to<std::vector>();
}

consteval std::vector<std::meta::info> candidates_for(std::meta::info member, std::meta::info type, const member_info& info) {
    const auto args = arg_types_for(member, info);

    return self_types_for(member, type)
        | std::views::transform([=](auto self) {
            std::vector targs{reflect_constant(member), self};
            targs.append_range(args);

            if (uses_explicit_object(member)) {
                return substitute(^^explict_obj_candidate, targs);
            }
            return substitute(^^function_candidate, targs);
        })
        | std::ranges::to<std::vector>();
}

consteval std::meta::info make_set(std::meta::info type, const member_info& info) {
    const std::string_view identifier{info.identifier};

    return substitute(^^overload_set, all_members_of(type)
        | std::views::filter(std::meta::has_identifier)
        | std::views::filter([identifier](auto member) {
            return identifier_of(member) == identifier;
        })
        | std::views::filter([](auto member) {
            return is_function(member) || is_invocable_field(member);
        })
        | std::views::transform([=](auto member) {
            return candidates_for(member, type, info);
        })
        | std::views::join
    );
}

template <member_info Info, bool Noexcept, typename RefType, auto CheckRet, typename... Args>
concept check_member_func = std::invoke([] {
    using overload_set_t = [: make_set(
        decay(^^RefType),
        Info) :];

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