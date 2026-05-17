#pragma once

#include "bind_type_trait.hpp"

namespace rjk {
    template <typename Func>
    struct remove_noexcept_trait { using type = Func; };

    template <typename Ret, typename... Args>
    struct remove_noexcept_trait<Ret(Args...) noexcept>             { using type = Ret(Args...); };
    template <typename Ret, typename... Args>
    struct remove_noexcept_trait<Ret(Args...) const noexcept>       { using type = Ret(Args...) const; };
    template <typename Ret, typename... Args>
    struct remove_noexcept_trait<Ret(Args...) & noexcept>           { using type = Ret(Args...) &; };
    template <typename Ret, typename... Args>
    struct remove_noexcept_trait<Ret(Args...) const & noexcept>     { using type = Ret(Args...) const &; };
    template <typename Ret, typename... Args>
    struct remove_noexcept_trait<Ret(Args...) && noexcept>          { using type = Ret(Args...) &&; };
    template <typename Ret, typename... Args>
    struct remove_noexcept_trait<Ret(Args...) const && noexcept>    { using type = Ret(Args...) const &&; };

    template <typename T>
    using remove_noexcept_t = remove_noexcept_trait<T>::type;
    
    consteval std::meta::info remove_noexcept(std::meta::info type) {
        return bind_type_trait<remove_noexcept_t>(type);
    }
}