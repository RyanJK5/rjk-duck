#ifndef RJK_META_UTIL_HPP
#define RJK_META_UTIL_HPP

#include <meta>
#include <ranges>

namespace rjk::detail {

// goes through all members of a class, including its base classes' members.
consteval std::vector<std::meta::info> all_members_of(std::meta::info class_type) {
    const auto ctx = std::meta::access_context::unprivileged();
    return std::views::concat(std::views::single(class_type), bases_of(class_type, ctx))
        | std::views::transform([ctx](auto class_t) {
            if (!is_complete_type(class_t)) {
                class_t = type_of(class_t);
            }
            return members_of(class_t, ctx);
        })
        | std::views::join
        | std::ranges::to<std::vector>();
}
}

#endif // RJK_META_UTIL_HPP
