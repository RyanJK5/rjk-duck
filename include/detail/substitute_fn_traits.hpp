#ifndef RJK_SUBSTITUTE_FN_ARGS_HPP
#define RJK_SUBSTITUTE_FN_ARGS_HPP

#include "flag_enum.hpp"
#include <meta>
#include <type_traits>

namespace rjk::detail {
template <typename Arg, typename Func>
struct prepend_arg;

template <typename Arg, typename Ret, typename... Args>
struct prepend_arg<Arg, Ret(Args...)> {
    using type = Ret(Arg, Args...);
};

template <typename Arg, typename Func>
using prepend_arg_t = prepend_arg<Arg, Func>::type;

template <typename Arg, typename Func>
struct append_arg;

template <typename Arg, typename Ret, typename... Args>
struct append_arg<Arg, Ret(Args...)> {
    using type = Ret(Args..., Arg);
};

template <typename Arg, typename Func>
using append_arg_t = append_arg<Arg, Func>::type;

}

namespace rjk::detail {
template <typename Func, typename Remove>
struct fn_remove_arg;

template <typename Ret, typename Arg, typename Remove>
struct fn_remove_arg<Ret(Arg), Remove> {
    using type = std::conditional_t<
        std::is_same_v<std::remove_cvref_t<Arg>, Remove>,
        Ret(),
        Ret(Arg)
    >;
};

template <typename Ret, typename Arg1, typename Arg2, typename Remove>
struct fn_remove_arg<Ret(Arg1, Arg2), Remove> {
    using type = std::conditional_t<
        std::is_same_v<std::remove_cvref_t<Arg1>, Remove>,
        Ret(Arg2),
        std::conditional_t<
            std::is_same_v<std::remove_cvref_t<Arg2>, Remove>,
            Ret(Arg1),
            Ret(Arg1, Arg2)
        >
    >;
};

template <typename Func, typename Remove>
using fn_remove_arg_t = fn_remove_arg<Func, Remove>::type;

consteval std::meta::info remove_arg(std::meta::info func,
                                     std::meta::info to_remove) {
    return dealias(substitute(^^fn_remove_arg_t, {func, to_remove}));
}
}

#endif