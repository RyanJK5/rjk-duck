// clang-format off

#ifndef RJK_META_UTIL_HPP
#define RJK_META_UTIL_HPP

#include <functional>
#include <meta>
#include <ranges>

namespace rjk::detail {

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
}

#endif // RJK_META_UTIL_HPP
