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
struct candidate_wrapper {
    constexpr decltype(auto) operator()(Self self, Args... args) const
    noexcept(noexcept(std::invoke(&[:Callable:], std::declval<Self>(), std::declval<Args>()...))) {
        return std::invoke(&[:Callable:], std::forward<Self>(self), std::forward<Args>(args)...);
    }
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
    std::string_view identifier) {
    return substitute(^^overload_set,
        all_members_of(type)
        | std::views::filter(std::meta::is_function)
        | std::views::filter(std::meta::has_identifier)
        | std::views::filter([identifier](auto member) {
            return identifier_of(member) == identifier;
        })
        | std::views::transform([=](auto member) {
            const auto params = parameters_of(member);
            return self_types_for(member, type)
                | std::views::transform([=](auto self) {
                    std::vector args{
                        reflect_constant(member),
                        self
                    };
                    args.append_range(params | std::views::transform(std::meta::type_of));
                    return substitute(^^candidate_wrapper, args);
                })
                | std::ranges::to<std::vector>();
        })
        | std::views::join
    );
}

template <fixed_string Identifier, bool Noexcept, typename RefType, auto CheckRet, typename... Args>
concept check_member_func = std::invoke([] {
    using overload_set_t = [: make_set(
        decay(^^RefType),
        std::string_view{Identifier}) :];

    constexpr static auto matches = requires (overload_set_t caller, RefType obj, Args&&... args) {
        { caller(static_cast<RefType>(obj), std::forward<Args>(args)...) } -> evaluate<CheckRet>;
    };

    if constexpr (matches) {
        return !Noexcept || noexcept(std::declval<overload_set_t&>()(
            std::declval<RefType>(), std::declval<Args>()...));
    }
    return false;
});

}

#endif // RJK_OVERLOAD_RESOLUTION_HPP
