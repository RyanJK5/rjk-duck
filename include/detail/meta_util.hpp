// clang-format off

#ifndef RJK_META_UTIL_HPP
#define RJK_META_UTIL_HPP

#include <functional>
#include <meta>
#include <ranges>

namespace rjk::detail {

// Searches the given type using search_func, and also all of the bases of that
// type.
consteval std::vector<std::meta::info> recursive_search(std::meta::info type, auto search_func, auto... args) {
    auto result = search_func(type, args...);
    auto bases = bases_of(type, args...);
    auto base_results = bases
        | std::views::transform(std::meta::type_of)
        | std::views::transform([=](std::meta::info type) {
            return recursive_search(type, search_func, args...);
        })
        | std::views::join;
    result.append_range(base_results);

    return result;
}

// goes through all members of a class, including its base classes' members.
consteval std::vector<std::meta::info> all_members_of(std::meta::info class_type) {
    const auto ctx = std::meta::access_context::unprivileged();

    return recursive_search(class_type, std::meta::members_of, ctx);
}
}

#endif // RJK_META_UTIL_HPP
