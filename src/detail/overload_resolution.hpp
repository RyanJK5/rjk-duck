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

// Stands in for a deleted member function. It stays in the overload set so it
// can still win overload resolution (as the real deleted function would), but
// calling it is ill-formed in the immediate context, so the check is false
// rather than a hard error from instantiating a forwarding body.
template <typename Self, typename... Args>
struct deleted_candidate {
    void operator()(Self self, Args... args) const = delete;
};

// Stands in for a deleted member function when choosing between the
// non-template and template overload sets: a deleted overload is still viable
// in overload resolution, so it must keep templates from being considered.
// Only ever used in unevaluated operands, so it needs no definition.
template <typename Self, typename... Args>
struct viable_candidate {
    void operator()(Self self, Args... args) const;
};

// Candidate for a member function template. Its parameter types aren't known
// until deduction, so it forwards to the real call and is only viable when
// that call compiles.
template <std::meta::info Callable>
struct template_candidate {
    template <typename Self, typename... Args>
        requires requires {
            std::declval<Self>().template [:Callable:](std::declval<Args>()...);
        }
    constexpr decltype(auto) operator()(Self&& self, Args&&... args) const
    noexcept(noexcept(std::declval<Self>().template [:Callable:](std::declval<Args>()...))) {
        return std::forward<Self>(self).template [:Callable:](std::forward<Args>(args)...);
    }
};

consteval bool is_invocable_field(std::meta::info member) {
    if (!is_variable(member) && !is_nonstatic_data_member(member)) {
        return false;
    }

    return is_function_type(remove_reference(type_of(member))) ||
        is_function_type(remove_pointer(type_of(member)));
}

consteval std::meta::info self_type_for(std::meta::info member, std::meta::info type) {
    if (is_function(member)) {
        const auto params = parameters_of(member);
        if (!params.empty() && is_explicit_object_parameter(params[0])) {
            return type_of(params[0]);
        }
    }

    const auto base = (is_static_member(member) || is_nonstatic_data_member(member) || is_const(member))
        ? add_const(type) : type;

    if (is_rvalue_reference_qualified(member)) {
        return add_rvalue_reference(base);
    } else {
        return add_lvalue_reference(base);
    }
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

consteval std::meta::info candidate_for(std::meta::info member, std::meta::info type,
    const member_info& info, bool viability_probe) {
    const auto args = arg_types_for(member, info);

    const auto self = self_type_for(member, type);
    std::vector targs{reflect_constant(member), self};
    targs.append_range(args);

    if (is_function(member) && is_deleted(member)) {
        return substitute(viability_probe ? ^^viable_candidate : ^^deleted_candidate,
            std::span{targs}.subspan(1));
    }
    if (uses_explicit_object(member)) {
        return substitute(^^explict_obj_candidate, targs);
    }
    return substitute(^^function_candidate, targs);
}

enum class overload_kind {
    non_template,
    non_template_probe,
    templated,
};

consteval std::meta::info make_set(std::meta::info type, const member_info& info, overload_kind kind) {
    const std::string_view identifier{info.identifier};

    std::vector<std::meta::info> candidates;
    for (const auto member : all_members_of(type)) {
        if (!has_identifier(member) || identifier_of(member) != identifier) {
            continue;
        }

        if (is_function_template(member)) {
            if (kind == overload_kind::templated) {
                candidates.push_back(substitute(^^template_candidate, {reflect_constant(member)}));
            }
        } else if (kind != overload_kind::templated
            && (is_function(member) || is_invocable_field(member))) {
            candidates.push_back(candidate_for(member, type, info,
                kind == overload_kind::non_template_probe));
        }
    }
    return substitute(^^overload_set, candidates);
}

template <typename Set, typename RefType, typename... Args>
concept callable_set =
    requires (Set set, RefType obj, Args&&... args) {
        set(std::forward<RefType>(obj), std::forward<Args>(args)...);
    } || requires (Set set, Args&&... args) {
        set(std::forward<Args>(args)...);
    };

// Picks the overloads a call through the trait can reach: the non-template
// overloads if any of them is callable with the trait's arguments, otherwise the
// member templates. Mixing both in one set would rank them wrongly, since a
// template candidate's forwarding parameters always bind exactly. The
// conformance check and the vtable both go through this, so they always agree.
consteval std::meta::info choose_set(std::meta::info type, const member_info& info,
    std::meta::info ref_type, std::span<const std::meta::info> args) {
    std::vector targs{make_set(type, info, overload_kind::non_template_probe), ref_type};
    targs.append_range(args);

    if (extract<bool>(substitute(^^callable_set, targs))) {
        return make_set(type, info, overload_kind::non_template);
    }
    return make_set(type, info, overload_kind::templated);
}

template <member_info Info, bool Noexcept, typename RefType, auto CheckRet, typename... Args>
concept check_member_func = [] consteval {
    using overload_set_t = [: choose_set(
        decay(^^RefType),
        Info,
        ^^RefType,
        std::vector<std::meta::info>{^^Args...}) :];

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
}();

}

#endif // RJK_OVERLOAD_RESOLUTION_HPP