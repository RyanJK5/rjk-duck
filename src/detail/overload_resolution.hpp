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

// Calls the probe set `Probes` (an overload_set of probes) with the trait's
// object and arguments, and gives the winning probe's index, or no_overload if
// none can be called or the call is ambiguous. Used by winning_overload.
template <typename Probes, typename RefType, typename... Args>
constexpr std::size_t probe_call_winner = [] {
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

// Ranks an impl member template whose only template parameter is its object
// argument, e.g. `static auto foo(const auto& self, int)`. The probe is itself a
// template whose object argument has the same form (only the overload for
// `Form` is enabled), so the compiler orders overloads like `auto&&` vs
// `const auto&` by partial ordering, as it would the real ones, while the other
// parameters are ranked by their concrete types.
enum class self_form {
    forwarding,   // auto&&
    lvalue,       // auto&
    const_lvalue, // const auto&
    value,        // auto
};

template <std::size_t I, typename Obj, self_form Form, typename... Params>
struct self_form_probe {
    template <typename S> requires (Form == self_form::forwarding)
    auto operator()(Obj, S&&, Params...) const -> overload_tag<I>;

    template <typename S> requires (Form == self_form::lvalue)
    auto operator()(Obj, S&, Params...) const -> overload_tag<I>;

    template <typename S> requires (Form == self_form::const_lvalue)
    auto operator()(Obj, const S&, Params...) const -> overload_tag<I>;

    template <typename S> requires (Form == self_form::value)
    auto operator()(Obj, S, Params...) const -> overload_tag<I>;
};

// Calls an impl member with member syntax on a fresh impl object, with the
// duck's object as the first argument. Static members can be called this way
// too, so impl members are looked up and called like the members of any type.
template <typename Impl, typename Call>
struct impl_adapter {
    template <typename Obj, typename... Args>
        requires requires {
            Call{}(std::declval<Impl&>(), std::declval<Obj>(), std::declval<Args>()...);
        }
    constexpr decltype(auto) operator()(Obj&& obj, Args&&... args) const
    noexcept(std::is_nothrow_default_constructible_v<Impl>
        && noexcept(Call{}(std::declval<Impl&>(), std::declval<Obj>(), std::declval<Args>()...))) {
        Impl impl{};
        return Call{}(impl, std::forward<Obj>(obj), std::forward<Args>(args)...);
    }
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

// Which of `probes` a call with the trait's object and arguments picks: their
// index, or no_overload. Builds the probe set and evaluates probe_call_winner.
consteval std::size_t winning_overload(std::span<const std::meta::info> probes,
    std::meta::info ref_type, std::span<const std::meta::info> args) {
    std::vector targs{substitute(^^overload_set, probes), ref_type};
    targs.append_range(args);
    return extract<std::size_t>(substitute(^^probe_call_winner, targs));
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

// The form-matching probe for an impl template whose only template parameter is
// its object argument, if it is one of the forms above. The form is read from
// the first parameter after substituting the object type: `T&&` comes from
// `auto&&`, `T&` from `auto&`, and so on.
consteval std::optional<std::meta::info> self_form_probe_for(std::size_t index,
    std::meta::info member, std::meta::info impl, std::meta::info type, const member_info& info) {
    if (!can_substitute(member, {type})) {
        return std::nullopt;
    }
    const auto type_fn = substitute(member, {type});
    if (!is_function(type_fn) || parameters_of(type_fn).empty()) {
        return std::nullopt;
    }

    const auto first = dealias(type_of(parameters_of(type_fn)[0]));
    const auto form = [&] -> std::optional<self_form> {
        if (first == add_rvalue_reference(type)) {
            return self_form::forwarding;
        } else if (first == add_lvalue_reference(type)) {
            return self_form::lvalue;
        } else if (first == add_lvalue_reference(add_const(type))) {
            return self_form::const_lvalue;
        } else if (first == type) {
            return self_form::value;
        }
        return std::nullopt;
    }();
    if (!form) {
        return std::nullopt;
    }

    std::vector targs{
        std::meta::reflect_constant(index),
        self_type_for(member, impl),
        std::meta::reflect_constant(*form)
    };
    targs.append_range(arg_types_for(type_fn, info) | std::views::drop(1));
    return substitute(^^self_form_probe, targs);
}

// An impl function's parameters are the object followed by the trait's
// arguments, so it's looked up with one more parameter than the trait has.
consteval member_info impl_member_info(const member_info& info) {
    return {.identifier = info.identifier, .param_count = info.param_count + 1};
}

// The call through `impl` for one of its members, given the duck's object type.
consteval std::meta::info impl_call_for(std::meta::info impl, std::meta::info member,
    std::meta::info self, std::span<const std::meta::info> args) {
    std::vector impl_args{self};
    impl_args.append_range(args);
    return substitute(^^impl_adapter, {impl, call_for(member, add_lvalue_reference(impl), impl_args)});
}

// Like choose_set, for an impl specialization: its members are called on an
// impl object, with the duck's object as the first argument.
consteval std::meta::info choose_impl_set(std::meta::info impl, std::meta::info type,
    const member_info& info, std::meta::info self, std::span<const std::meta::info> args) {
    const auto impl_info = impl_member_info(info);
    const auto overloads = overloads_named(impl, impl_info);

    std::vector<std::meta::info> probes;
    for (std::size_t i{}; i < overloads.size(); ++i) {
        const auto form_probe = is_function_template(overloads[i])
            ? self_form_probe_for(i, overloads[i], impl, type, impl_info)
            : std::nullopt;
        probes.push_back(form_probe ? *form_probe : probe_for(i, overloads[i], impl, impl_info));
    }

    std::vector impl_args{self};
    impl_args.append_range(args);
    const auto index = winning_overload(probes, add_lvalue_reference(impl), impl_args);
    if (index == no_overload) {
        return ^^overload_set<>;
    }
    return impl_call_for(impl, overloads[index], self, args);
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