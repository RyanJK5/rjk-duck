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

// Whether a candidate for `member` has to call it through a pointer, which
// can't use default arguments: an explicit object member whose object
// parameter isn't the class that declares it.
consteval bool calls_through_pointer(std::meta::info member) {
    return uses_explicit_object(member)
        && decay(type_of(parameters_of(member)[0])) != parent_of(member);
}

consteval bool is_invocable_field(std::meta::info member) {
    if (!is_variable(member) && !is_nonstatic_data_member(member)) {
        return false;
    }

    return is_function_type(remove_reference(type_of(member)))
        || is_function_type(remove_pointer(type_of(member)));
}

consteval std::vector<std::meta::info> arg_types_for(std::meta::info member, const member_info& info) {
    if (is_invocable_field(member)) {
        return parameters_of(remove_pointer(decay(type_of(member))));
    }

    // The explicit object parameter is the candidate's Self, not an argument.
    const auto fullParams = [&] {
        auto params = parameters_of(member);
        if (uses_explicit_object(params)) {
            params.erase(params.begin());
        }
        return params;
    }();

    // A call through a pointer can't use default arguments, so keep them all.
    if (calls_through_pointer(member)) {
        return fullParams
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

// {Self, Args...}: the object type and parameter types a candidate for `member`
// is called with.
consteval std::vector<std::meta::info> candidate_parameters(std::meta::info member,
    std::meta::info type, const member_info& info) {
    std::vector signature{self_type_for(member, type)};
    signature.append_range(arg_types_for(member, info));
    return signature;
}

// Calls with member syntax, which uses default arguments.
template <std::meta::info Callable, typename Self, typename... Args>
struct function_candidate {
    constexpr decltype(auto) operator()(Self self, Args... args) const
    noexcept(noexcept(std::declval<Self>().[:Callable:](std::declval<Args>()...))) {
        return std::forward<Self>(self).[:Callable:](std::forward<Args>(args)...);
    }
};

// For explicit object members whose object parameter isn't the class itself
// (a base class, or a type the class converts to): member syntax can't be used
// on that type, so this calls through a pointer, without default arguments.
template <std::meta::info Callable, typename Self, typename... Args>
struct explict_obj_candidate {
    constexpr decltype(auto) operator()(Self self, Args... args) const
    noexcept(noexcept(std::invoke(&[:Callable:], std::declval<Self>(), std::declval<Args>()...))) {
        return std::invoke(&[:Callable:], std::forward<Self>(self), std::forward<Args>(args)...);
    }
};

consteval std::meta::info function_candidate_for(std::meta::info member,
    std::meta::info type, const member_info& info) {
    std::vector targs{reflect_constant(member)};
    targs.append_range(candidate_parameters(member, type, info));
    if (calls_through_pointer(member)) {
        return substitute(^^explict_obj_candidate, targs);
    }
    return substitute(^^function_candidate, targs);
}

// Stands in for a deleted member function. It stays in the overload set so it
// can still win overload resolution (as the real deleted function would), but
// calling it is ill-formed in the immediate context, so the check is false
// rather than a hard error from instantiating a forwarding body.
template <typename Self, typename... Args>
struct deleted_candidate {
    void operator()(Self self, Args... args) const = delete;
};

consteval std::meta::info deleted_candidate_for(std::meta::info member,
    std::meta::info type, const member_info& info) {
    return substitute(^^deleted_candidate, candidate_parameters(member, type, info));
}

// Stands in for a deleted member function when choosing between the
// non-template and template overload sets: a deleted overload is still viable
// in overload resolution, so it must keep templates from being considered.
// Only ever used in unevaluated operands, so it needs no definition.
template <typename Self, typename... Args>
struct viable_candidate {
    void operator()(Self self, Args... args) const;
};

consteval std::meta::info viable_candidate_for(std::meta::info member,
    std::meta::info type, const member_info& info) {
    return substitute(^^viable_candidate, candidate_parameters(member, type, info));
}

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

consteval std::meta::info template_candidate_for(std::meta::info member) {
    return substitute(^^template_candidate, {std::meta::reflect_constant(member)});
}

consteval std::vector<std::meta::info> members_named(std::meta::info type, const member_info& info) {
    return all_members_of(type)
        | std::views::filter(std::meta::has_identifier)
        | std::views::filter([id = std::string_view{info.identifier}](std::meta::info member) {
            return identifier_of(member) == id;
        })
        | std::ranges::to<std::vector>();
}

// The members a non-template candidate can be made for: functions and fields
// holding something callable.
consteval std::vector<std::meta::info> non_template_members(std::meta::info type, const member_info& info) {
    return members_named(type, info)
        | std::views::filter([](std::meta::info member) {
            return is_function(member) || is_invocable_field(member);
        })
        | std::ranges::to<std::vector>();
}

consteval std::meta::info make_non_template_set(std::meta::info type, const member_info& info) {
    return substitute(^^overload_set, non_template_members(type, info)
        | std::views::transform([=](std::meta::info member) {
            if (is_deleted(member)) {
                return deleted_candidate_for(member, type, info);
            } else {
                return function_candidate_for(member, type, info);
            }
        }));
}

consteval std::meta::info make_viability_probe(std::meta::info type, const member_info& info) {
    return substitute(^^overload_set, non_template_members(type, info)
        | std::views::transform([=](std::meta::info member) {
            if (is_deleted(member)) {
                return viable_candidate_for(member, type, info);
            } else {
                return function_candidate_for(member, type, info);
            }
        }));
}

consteval std::meta::info make_template_set(std::meta::info type, const member_info& info) {
    return substitute(^^overload_set, members_named(type, info)
        | std::views::filter(std::meta::is_function_template)
        | std::views::transform([=](std::meta::info member) {
            return template_candidate_for(member);
        }));
}

template <typename OverloadSet, typename RefType, typename... Args>
concept callable_set =
    requires (OverloadSet set, RefType obj, Args&&... args) {
        set(std::forward<RefType>(obj), std::forward<Args>(args)...);
    } || requires (OverloadSet set, Args&&... args) {
        set(std::forward<Args>(args)...);
    };

// Picks the overloads a call through the trait can reach: the non-template
// overloads if any of them is callable with the trait's arguments, otherwise the
// member templates. Mixing both in one set would rank them wrongly, since a
// template candidate's forwarding parameters always bind exactly. The
// conformance check and the vtable both go through this, so they always agree.
consteval std::meta::info choose_set(std::meta::info type, const member_info& info,
    std::meta::info ref_type, std::span<const std::meta::info> args) {
    std::vector targs{make_viability_probe(type, info), ref_type};
    targs.append_range(args);

    if (extract<bool>(substitute(^^callable_set, targs))) {
        return make_non_template_set(type, info);
    } else {
        return make_template_set(type, info);
    }
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