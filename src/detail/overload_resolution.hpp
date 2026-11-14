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

    const auto fullParams = [&] {
        auto params = parameters_of(member);
        if (uses_explicit_object(params)) {
            // The explicit object parameter is the candidate's Self, not an argument.
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
// a single overload set, and the compiler chooses the best one using C++ overload
// resolution rules. Then, the winning candidate returns its index, and we create
// a call candidate for that one overload to use as the canonical dispatch function.

// Why? This allows us to gracefully handle some subtleties when ranking template
// and non-template overloads against each other, as well as with explicit object
// parameters.

template <std::size_t I>
struct overload_tag {
    static constexpr std::size_t index{I};
};

// Ranks a non-template overload by its own object and parameter types. A
// deleted overload is ranked too: if it wins, its call candidate fails.
template <std::size_t I, typename Self, typename... Params>
struct overload_probe {
    auto operator()(Self, Params...) const -> overload_tag<I>;
};

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

// Calls a static member of an impl specialization, which takes the object as
// its first parameter. A direct call uses default arguments.
template <std::meta::info Callable, typename Obj, typename... Args>
struct impl_call {
    constexpr decltype(auto) operator()(Obj obj, Args... args) const
    noexcept(noexcept([:Callable:](std::declval<Obj>(), std::declval<Args>()...))) {
        return [:Callable:](std::forward<Obj>(obj), std::forward<Args>(args)...);
    }
};

template <std::meta::info Callable, typename Obj, typename... Args>
struct impl_template_call {
    constexpr decltype(auto) operator()(Obj obj, Args... args) const
    noexcept(noexcept(template [:Callable:](std::declval<Obj>(), std::declval<Args>()...))) {
        return template [:Callable:](std::forward<Obj>(obj), std::forward<Args>(args)...);
    }
};

// Ranks an impl member template whose only template parameter is its object
// parameter, e.g. `static auto foo(const auto& self, int)`. The probe is itself
// a template with the same form of object parameter, so the compiler orders
// overloads like `auto&&` vs `const auto&` by partial ordering, as it would the
// real ones, while the other parameters are ranked by their concrete types.
template <std::size_t I, typename... Params>
struct forwarding_self_probe {
    template <typename S>
    auto operator()(S&&, Params...) const -> overload_tag<I>;
};

template <std::size_t I, typename... Params>
struct lvalue_self_probe {
    template <typename S>
    auto operator()(S&, Params...) const -> overload_tag<I>;
};

template <std::size_t I, typename... Params>
struct const_lvalue_self_probe {
    template <typename S>
    auto operator()(const S&, Params...) const -> overload_tag<I>;
};

template <std::size_t I, typename... Params>
struct value_self_probe {
    template <typename S>
    auto operator()(S, Params...) const -> overload_tag<I>;
};

// Ranks any other impl member template, like template_probe but called with
// the object as the first argument.
template <std::size_t I, std::meta::info Callable>
struct impl_template_probe {
    template <typename Self, typename... Args>
        requires requires {
            template [:Callable:](std::declval<Self>(), std::declval<Args>()...);
        }
    auto operator()(Self&&, Args&&...) const -> overload_tag<I>;
};

// Stands in for a deleted member that won overload resolution.
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

// The index of the probe a call with these arguments picks, or no_overload.
consteval std::size_t winning_overload(std::span<const std::meta::info> probes,
    std::meta::info ref_type, std::span<const std::meta::info> args) {
    std::vector targs{substitute(^^overload_set, probes), ref_type};
    targs.append_range(args);
    return extract<std::size_t>(substitute(^^chosen_overload, targs));
}

consteval std::meta::info choose_set(std::meta::info type, const member_info& info,
    std::meta::info ref_type, std::span<const std::meta::info> args) {
    const auto overloads = overloads_named(type, info);

    std::vector<std::meta::info> probes;
    for (std::size_t i{}; i < overloads.size(); ++i) {
        probes.push_back(probe_for(i, overloads[i], type, info));
    }

    const auto index = winning_overload(probes, ref_type, args);
    if (index == no_overload) {
        return ^^overload_set<>;
    }
    return call_for(overloads[index], ref_type, args);
}

// The static members of an impl specialization with info's name.
consteval std::vector<std::meta::info> impl_overloads_named(std::meta::info impl, const member_info& info) {
    return members_of(impl, std::meta::access_context::unprivileged())
        | std::views::filter(std::meta::is_static_member)
        | std::views::filter(std::meta::has_identifier)
        | std::views::filter([id = std::string_view{info.identifier}](std::meta::info member) {
            return identifier_of(member) == id;
        })
        | std::views::filter([](std::meta::info member) {
            return is_function(member) || is_function_template(member);
        })
        | std::ranges::to<std::vector>();
}

// An impl function's parameters are its object parameter followed by the
// trait's arguments, so it's trimmed to one more parameter than the trait has.
consteval std::vector<std::meta::info> impl_parameters(std::meta::info member, const member_info& info) {
    return arg_types_for(member, {.identifier = info.identifier, .param_count = info.param_count + 1});
}

// The form-matching probe for an impl template whose only template parameter is
// its object parameter, if it is one of the forms above. The form is read from
// the object parameter after substituting the object type: `T&&` comes from
// `auto&&`, `T&` from `auto&`, and so on.
consteval std::optional<std::meta::info> self_form_probe_for(std::meta::info tag,
    std::meta::info member, std::meta::info type, const member_info& info) {
    if (!can_substitute(member, {type})) {
        return std::nullopt;
    }
    const auto type_fn = substitute(member, {type});
    if (!is_function(type_fn) || parameters_of(type_fn).empty()) {
        return std::nullopt;
    }

    const auto form = dealias(type_of(parameters_of(type_fn)[0]));
    const auto probe = [&] -> std::optional<std::meta::info> {
        if (form == add_rvalue_reference(type)) {
            return ^^forwarding_self_probe;
        } else if (form == add_lvalue_reference(type)) {
            return ^^lvalue_self_probe;
        } else if (form == add_lvalue_reference(add_const(type))) {
            return ^^const_lvalue_self_probe;
        } else if (form == type) {
            return ^^value_self_probe;
        }
        return std::nullopt;
    }();
    if (!probe) {
        return std::nullopt;
    }

    std::vector targs{tag};
    targs.append_range(impl_parameters(type_fn, info) | std::views::drop(1));
    return substitute(*probe, targs);
}

// The probe for an impl member, or none if it can't take the object (it has no
// parameters).
consteval std::optional<std::meta::info> impl_probe_for(std::size_t index, std::meta::info member,
    std::meta::info type, const member_info& info) {
    const auto tag = std::meta::reflect_constant(index);
    if (is_function_template(member)) {
        if (const auto probe = self_form_probe_for(tag, member, type, info)) {
            return probe;
        }
        return substitute(^^impl_template_probe, {tag, std::meta::reflect_constant(member)});
    }

    const auto params = impl_parameters(member, info);
    if (params.empty()) {
        return std::nullopt;
    }
    std::vector targs{tag};
    targs.append_range(params);
    return substitute(^^overload_probe, targs);
}

consteval std::meta::info impl_call_for(std::meta::info member,
    std::meta::info ref_type, std::span<const std::meta::info> args) {
    if (is_function(member) && is_deleted(member)) {
        std::vector targs{ref_type};
        targs.append_range(args);
        return substitute(^^deleted_call, targs);
    }

    std::vector targs{std::meta::reflect_constant(member), ref_type};
    targs.append_range(args);
    return substitute(is_function_template(member) ? ^^impl_template_call : ^^impl_call, targs);
}

// Like choose_set, for the members of an impl specialization.
consteval std::meta::info choose_impl_set(std::meta::info impl, std::meta::info type,
    const member_info& info, std::meta::info ref_type, std::span<const std::meta::info> args) {
    std::vector<std::meta::info> overloads;
    std::vector<std::meta::info> probes;
    for (const auto member : impl_overloads_named(impl, info)) {
        if (const auto probe = impl_probe_for(overloads.size(), member, type, info)) {
            overloads.push_back(member);
            probes.push_back(*probe);
        }
    }

    const auto index = winning_overload(probes, ref_type, args);
    if (index == no_overload) {
        return ^^overload_set<>;
    }
    return impl_call_for(overloads[index], ref_type, args);
}

// Whether calling the chosen set with the trait's object and arguments
// compiles, returns something compatible (CheckRet) and, for a noexcept trait
// member, is noexcept.
template <std::meta::info Set, bool Noexcept, typename RefType, auto CheckRet, typename... Args>
concept set_satisfies = [] consteval {
    using overload_set_t = [:Set:];

    constexpr static auto matches =
        requires (overload_set_t caller, RefType obj, Args&&... args) {
            { caller(std::forward<RefType>(obj), std::forward<Args>(args)...) } -> evaluate<CheckRet>;
        };

    if constexpr (matches) {
        return !Noexcept || noexcept(std::declval<overload_set_t&>()(
            std::declval<RefType>(), std::declval<Args>()...));
    }
    return false;
}();

}

#endif // RJK_OVERLOAD_RESOLUTION_HPP