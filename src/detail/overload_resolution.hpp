#ifndef RJK_OVERLOAD_RESOLUTION_HPP
#define RJK_OVERLOAD_RESOLUTION_HPP

#include "fixed_string.hpp"

#include <meta>
#include <ranges>
#include <vector>

#include "detail/fixed_string.hpp"
#include "detail/meta_utils.hpp"

namespace rjk::detail {

template <typename... CandidateWrappers>
struct overload_resolver : CandidateWrappers... {
    using CandidateWrappers::operator()...;
    using CandidateWrappers::get_member_function...;
};

template <std::meta::info Callable, bool FreeCallSyntax, typename Self, typename... Args>
struct candidate_wrapper {
    constexpr decltype(auto) operator()(Self self, Args... args) const {
        if constexpr (FreeCallSyntax) {
            return [:Callable:](static_cast<Self>(self), std::forward<Args>(args)...);
        } else {
            return static_cast<Self>(self).[:Callable:](std::forward<Args>(args)...);
        }
    }

    consteval static std::meta::info get_member_function(Self self, Args... args) const {
        return Callable;
    }
};

consteval std::vector<std::meta::info> self_types_for(std::meta::info member, std::meta::info Type) {
    const auto base = is_const(member) ? add_const(Type) : Type;

    if (is_lvalue_reference_qualified(member)) {
        return { add_lvalue_reference(base) };
    }
    if (is_rvalue_reference_qualified(member)) {
        return { add_rvalue_reference(base) };
    }

    return { add_lvalue_reference(base), add_rvalue_reference(base) };
}

consteval std::meta::info make_set(std::meta::info type,
    std::string_view identifier, bool free_call_syntax) {
    return substitute(^^overload_resolver,
        members_of(type, std::meta::access_context::unprivileged())
        | std::views::filter(std::meta::is_function)
        | std::views::filter(std::meta::has_identifier)
        | std::views::filter([identifier](auto member) {
            return identifier_of(member) == identifier;
        })
        | std::views::transform([type](auto member) {
            return self_types_for(member, type)
            | std::views::transform([member, type](auto self) {
                std::vector args{reflect_constant(member), self, free_call_syntax};
                args.append_range(parameters_of(member)
                    | std::views::transform(std::meta::type_of));
                return substitute(^^candidate_wrapper, args);
            });
        })
        | std::views::join
    );
}

template <fixed_string Identifier, typename RefType, bool FreeCallSyntax, typename... Args>
consteval bool check_member_func() {
    using overload_set_t = typename [:
        make_set(^^RefType, std::string_view{Identifier}, FreeCallSyntax) :];
    return std::invocable<overload_set_t, RefType, Args...>;
}

template <fixed_string Identifier, typename Signature, typename RefType, bool FreeCallSyntax>
consteval std::meta::info get_member_func() {
    using overload_set_t = typename [:
        make_set(^^RefType, std::string_view{Identifier}, FreeCallSyntax) :];
    return overload_set_t::get_member_function(std::declval<RefType>(), std::declval<Args>()...);
}

}

#endif // RJK_OVERLOAD_RESOLUTION_HPP
