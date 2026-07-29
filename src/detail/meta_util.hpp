// clang-format off

#ifndef RJK_META_UTIL_HPP
#define RJK_META_UTIL_HPP

#include <functional>
#include <meta>
#include <ranges>

namespace rjk::detail {

template <typename T, typename U>
concept decays_to = std::same_as<std::decay_t<T>, U>;

template <typename... Callables>
struct overload_set : Callables... {
    using Callables::operator()...;
};

template <std::meta::info Func>
struct call_wrapper {
    template <typename... Args>
    constexpr decltype(auto) operator()(Args&&... args) const
        noexcept(noexcept([:Func:](std::declval<Args>()...))) {
        return [:Func:](std::forward<Args>(args)...);
    }
};

// Searches the given type using search_func, and also all of the bases of that
// type.
consteval std::vector<std::meta::info> recursive_search(std::meta::info type, auto search_func,
    std::meta::access_context ctx, auto... search_args) {
    auto result = search_func(type, search_args...);
    auto bases = bases_of(type, ctx);
    auto base_results = bases
        | std::views::transform(std::meta::type_of)
        | std::views::transform([=](std::meta::info type) {
            return recursive_search(type, search_func, ctx, search_args...);
        })
        | std::views::join;
    result.append_range(base_results);

    return result;
}

// goes through all members of a class, including its base classes' members.
consteval std::vector<std::meta::info> all_members_of(std::meta::info class_type) {
    const auto ctx = std::meta::access_context::unprivileged();

    return recursive_search(class_type, std::meta::members_of, ctx, ctx);
}

// returns a vector of the provided type and all of its bases, all the way up
// the inheritance chain.
consteval std::vector<std::meta::info> family_tree_for(std::meta::info class_type) {
    const auto ctx = std::meta::access_context::unprivileged();

    return recursive_search(class_type, [](auto base) { return std::vector{base}; }, ctx);
}

template <std::meta::info>
struct meta_wrapper_helper {};

// Copied from https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2026/p4032r1.html#implementation
consteval std::strong_ordering compare_meta_info(std::meta::info a, std::meta::info b) {
    constexpr static auto is_exactly_type = [](std::meta::info i){
        return is_type(i) && !is_type_alias(i);
    };
    if (is_exactly_type(a) && is_exactly_type(b)) {
        // ensure that on types meta::info ordering is consistent with type_order
        const auto ordering_info = substitute(
            ^^std::type_order_v, {a, b}
        );
        return extract<const std::strong_ordering&>(ordering_info);
    } else if (!is_exactly_type(a) && !is_exactly_type(b)) {
        // indirect through helper class template for non-type reflections
        const auto ordering_info = substitute(
            ^^std::type_order_v, {
                substitute(^^meta_wrapper_helper, {std::meta::reflect_constant(a)}),
                substitute(^^meta_wrapper_helper, {std::meta::reflect_constant(b)}),
            }
        );
        return extract<const std::strong_ordering&>(ordering_info);
    } else {
        // non-types compare less than types
        return is_exactly_type(a) <=> is_exactly_type(b);
    }
}
}

#endif
