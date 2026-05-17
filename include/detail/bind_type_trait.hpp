#pragma once

#include <meta>

template <template <typename...> typename Trait, typename... Args>
consteval std::meta::info bind_type_trait(std::meta::info type) {
    return dealias(substitute(^^Trait, {type, ^^Args...}));
}