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

// {Self, Args...}: the object type and parameter types an overload is ranked
// by. Self is the member's own object parameter type (as declared, for an
// explicit object member), so ranking matches the real overloads.
consteval std::vector<std::meta::info> candidate_parameters(std::meta::info member,
    std::meta::info type, const member_info& info) {
    std::vector signature{self_type_for(member, type)};
    signature.append_range(arg_types_for(member, info));
    return signature;
}

// Choosing an overload takes two steps. First, one probe per overload goes into
// a single overload set, and the compiler ranks them with the real rules; the
// winning probe's return type names the overload. Then a call candidate for
// that one overload calls it with member syntax on the original object, which
// lets the call convert the object to an explicit object parameter of another
// type and use default arguments.

template <std::size_t I>
struct overload_tag {
    static constexpr std::size_t index{I};
};

// Probes are only used in unevaluated operands, so they need no definitions.

// Ranks a non-template overload by its own object and parameter types. A
// deleted overload is ranked too: if it wins, its call candidate fails.
template <std::size_t I, typename Self, typename... Params>
struct overload_probe {
    auto operator()(Self, Params...) const -> overload_tag<I>;
};

// Ranks a member template. Its parameters can't be reflected, so it binds the
// object and every argument by reference, as a forwarding-reference template
// would, and is viable when the real call compiles.
template <std::size_t I, std::meta::info Callable>
struct template_probe {
    template <typename Self, typename... Args>
        requires requires {
            std::declval<Self>().template [:Callable:](std::declval<Args>()...);
        }
    auto operator()(Self&&, Args&&...) const -> overload_tag<I>;
};

inline constexpr std::size_t no_overload{static_cast<std::size_t>(-1)};

// The index of the overload a call with these arguments picks, or no_overload
// if none can be called or the call is ambiguous.
template <typename Probes, typename RefType, typename... Args>
constexpr std::size_t chosen_overload = [] {
    if constexpr (requires { std::declval<const Probes&>()(std::declval<RefType>(), std::declval<Args>()...); }) {
        return decltype(std::declval<const Probes&>()(
            std::declval<RefType>(), std::declval<Args>()...))::index;
    } else {
        return no_overload;
    }
}();

// Calls the chosen member with member syntax on the original object.
template <std::meta::info Callable, typename Obj, typename... Args>
struct member_call {
    constexpr decltype(auto) operator()(Obj obj, Args... args) const
    noexcept(noexcept(std::declval<Obj>().[:Callable:](std::declval<Args>()...))) {
        return std::forward<Obj>(obj).[:Callable:](std::forward<Args>(args)...);
    }
};

template <std::meta::info Callable, typename Obj, typename... Args>
struct member_template_call {
    constexpr decltype(auto) operator()(Obj obj, Args... args) const
    noexcept(noexcept(std::declval<Obj>().template [:Callable:](std::declval<Args>()...))) {
        return std::forward<Obj>(obj).template [:Callable:](std::forward<Args>(args)...);
    }
};

// Stands in for a deleted member that won overload resolution: calling it is
// ill-formed in the immediate context, so the check is false rather than a
// hard error.
template <typename Obj, typename... Args>
struct deleted_call {
    void operator()(Obj obj, Args... args) const = delete;
};

consteval std::vector<std::meta::info> members_named(std::meta::info type, const member_info& info) {
    return all_members_of(type)
        | std::views::filter(std::meta::has_identifier)
        | std::views::filter([id = std::string_view{info.identifier}](std::meta::info member) {
            return identifier_of(member) == id;
        })
        | std::ranges::to<std::vector>();
}

// The members that can be called with info's name: functions, function
// templates and fields holding something callable.
consteval std::vector<std::meta::info> overloads_named(std::meta::info type, const member_info& info) {
    return members_named(type, info)
        | std::views::filter([](std::meta::info member) {
            return is_function(member) || is_function_template(member) || is_invocable_field(member);
        })
        | std::ranges::to<std::vector>();
}

consteval std::meta::info probe_for(std::size_t index, std::meta::info member,
    std::meta::info type, const member_info& info) {
    const auto tag = std::meta::reflect_constant(index);
    if (is_function_template(member)) {
        return substitute(^^template_probe, {tag, std::meta::reflect_constant(member)});
    }
    std::vector targs{tag};
    targs.append_range(candidate_parameters(member, type, info));
    return substitute(^^overload_probe, targs);
}

consteval std::meta::info call_for(std::meta::info member,
    std::meta::info ref_type, std::span<const std::meta::info> args) {
    if (is_function(member) && is_deleted(member)) {
        std::vector targs{ref_type};
        targs.append_range(args);
        return substitute(^^deleted_call, targs);
    }

    std::vector targs{std::meta::reflect_constant(member), ref_type};
    targs.append_range(args);
    return substitute(is_function_template(member) ? ^^member_template_call : ^^member_call, targs);
}

// The callable a call through the trait reaches: a call candidate for the
// overload C++ would pick, or an empty set if there's none. The conformance
// check and the vtable both use this, so they always agree.
consteval std::meta::info choose_set(std::meta::info type, const member_info& info,
    std::meta::info ref_type, std::span<const std::meta::info> args) {
    const auto overloads = overloads_named(type, info);

    std::vector<std::meta::info> probes;
    for (std::size_t i{}; i < overloads.size(); ++i) {
        probes.push_back(probe_for(i, overloads[i], type, info));
    }

    std::vector targs{substitute(^^overload_set, probes), ref_type};
    targs.append_range(args);
    const auto index = extract<std::size_t>(substitute(^^chosen_overload, targs));
    if (index == no_overload) {
        return ^^overload_set<>;
    }
    return call_for(overloads[index], ref_type, args);
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