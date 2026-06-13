#ifndef RJK_FN_TRAITS_HPP
#define RJK_FN_TRAITS_HPP

#include <type_traits>
#include <meta>
#include <ranges>

namespace rjk::detail {

// Allows easy translation between concepts and meta-functions.
template <typename T, auto Func>
concept evaluate = Func(^^T);

template <typename Ret, typename... Args>
using make_func_t = Ret(Args...);

template <std::meta::info Ret, std::meta::info... Args>
using make_func_meta = make_func_t<typename [:Ret:], typename [:Args:]...>;

template <std::meta::reflection_range R = std::initializer_list<std::meta::info>>
consteval std::meta::info make_func(std::meta::info ret, R&& args) {
    std::vector template_args{ret};
    template_args.append_range(args);
    return dealias(substitute(^^make_func_meta,
        template_args | std::views::transform([](auto arg) {
            return reflect_constant(arg);
        })
    ));
}

template <std::meta::reflection_range R = std::initializer_list<std::meta::info>>
consteval std::meta::info make_func(R&& ret_and_args) {
    return dealias(substitute(^^make_func_meta,
        std::forward<R>(ret_and_args) | std::views::transform([](auto arg) {
            return reflect_constant(arg);
        })
    ));
}

consteval std::meta::info prepend_arg(std::meta::info to_prepend, std::meta::info func) {
    auto params = parameters_of(func);
    auto args = std::views::concat(
        std::views::single(return_type_of(func)),
        std::views::single(to_prepend),
        params
    );

    return make_func(args);
}

consteval std::meta::info append_arg(std::meta::info to_append, std::meta::info func) {
    std::vector args{return_type_of(func)};
    args.append_range(parameters_of(func));
    args.push_back(to_append);

    return make_func(args);
}

consteval std::meta::info remove_arg(std::meta::info func,
                                     std::meta::info to_remove) {
    auto params = parameters_of(func);
    return make_func(return_type_of(func), params
        | std::views::filter([to_remove](auto param) {
        return decay(param) != to_remove;
    }));
}
}

#endif