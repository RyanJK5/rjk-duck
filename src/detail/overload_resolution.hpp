#ifndef RJK_OVERLOAD_RESOLUTION_HPP
#define RJK_OVERLOAD_RESOLUTION_HPP

#include <concepts>
#include <meta>
#include <ranges>
#include <vector>

#include "detail/fixed_string.hpp"
#include "detail/fn_traits.hpp"
#include "detail/meta_util.hpp"

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

    consteval static std::integral_constant<std::meta::info, Callable>
        get_member_function(Self self, Args... args);
};

consteval std::vector<std::meta::info> self_types_for(std::meta::info member, std::meta::info type) {
    const auto base = is_const(member) ? add_const(type) : type;

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
        all_members_of(type)
        | std::views::filter(std::meta::is_function)
        | std::views::filter(std::meta::has_identifier)
        | std::views::filter([identifier](auto member) {
            return identifier_of(member) == identifier;
        })
        | std::views::transform([=](auto member) {
            return self_types_for(member, type)
            | std::views::transform([=](auto self) {
                std::vector args{
                    reflect_constant(member),
                    std::meta::reflect_constant(free_call_syntax), self
                };
                args.append_range(parameters_of(member)
                    | std::views::transform(std::meta::type_of));
                return substitute(^^candidate_wrapper, args);
            });
        })
        | std::views::join
    );
}

template <fixed_string Identifier, typename RefType, typename... Args>
consteval std::meta::info get_member_func() {
    using overload_set_t = typename [:
        make_set(decay(^^RefType), std::string_view{Identifier}, false) :];
    return decltype(std::declval<overload_set_t>()
        .get_member_function(std::declval<RefType>(), std::declval<Args>()...))::value;
}

template <fixed_string Identifier, bool Noexcept, typename RefType, auto CheckRet, typename... Args>
concept check_member_func = std::invoke([] {
    using overload_set_t = typename [:
        make_set(decay(^^RefType), std::string_view{Identifier}, false) :];

    constexpr static auto matches = requires (overload_set_t caller, RefType obj, Args&&... args) {
        { caller(static_cast<RefType>(obj), std::forward<Args>(args)...) } -> evaluate<CheckRet>;
    };

    if constexpr (matches) {
        constexpr static auto member = get_member_func<Identifier, RefType, Args...>();
        return !Noexcept ||
            noexcept(std::declval<RefType>().[:member:](std::declval<Args>()...));
    }
    return false;
});

}

#endif // RJK_OVERLOAD_RESOLUTION_HPP
