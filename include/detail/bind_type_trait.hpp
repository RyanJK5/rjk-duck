#ifndef RJK_BIND_TYPE_TRAIT_HPP
#define RJK_BIND_TYPE_TRAIT_HPP

#include <meta>

// Binds an existing type trait to a consteval "meta-function."
template <template <typename...> typename Trait, typename... Args>
consteval std::meta::info bind_type_trait(std::meta::info type) {
    return dealias(substitute(^^Trait, {type, ^^Args...}));
}

#endif