// clang-format off

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

// Creates a function type with the provided return type and arguments.
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

// Creates a function from a range, where the first element is the return value
// and the rest are arguments.
template <std::meta::reflection_range R = std::initializer_list<std::meta::info>>
consteval std::meta::info make_func(R&& ret_and_args) {
    return dealias(substitute(^^make_func_meta,
        std::forward<R>(ret_and_args) | std::views::transform([](auto arg) {
            return reflect_constant(arg);
        })
    ));
}


// Creates a function type with the given argument as the first.
consteval std::meta::info prepend_arg(std::meta::info to_prepend, std::meta::info func) {
    auto params = parameters_of(func);
    auto args = std::views::concat(
        std::views::single(return_type_of(func)),
        std::views::single(to_prepend),
        params
    );

    return make_func(args);
}

// Creates a function type with the given argument as the final parameter.
consteval std::meta::info append_arg(std::meta::info to_append, std::meta::info func) {
    std::vector args{return_type_of(func)};
    args.append_range(parameters_of(func));
    args.push_back(to_append);

    return make_func(args);
}

// Removes an an argument type from a function's parameter list.
consteval std::meta::info remove_arg(std::meta::info func,
                                     std::meta::info to_remove) {
    auto params = parameters_of(func);
    return make_func(return_type_of(func), params
        | std::views::filter([to_remove](auto param) {
        return decay(param) != to_remove;
    }));
}

template <typename T, typename... Args>
concept subscriptable = requires(T t, Args&&... args) {
    t[std::forward<Args>(args)...];
};

template <std::meta::reflection_range R = std::initializer_list<std::meta::info>>
consteval bool is_subscriptable(std::meta::info type, R&& parameters) {
    return extract<bool>(substitute(^^subscriptable,
        std::views::concat(std::views::single(type), std::forward<R>(parameters))));
}

template <typename T, typename... Args>
using subscript_result_t = decltype(std::declval<T>()[std::declval<Args>()...]);

template <std::meta::reflection_range R = std::initializer_list<std::meta::info>>
consteval std::meta::info subscript_result(std::meta::info type, R&& parameters) {
    return dealias(substitute(^^subscript_result_t,
        std::views::concat(std::views::single(type), std::forward<R>(parameters))));
}
}

#endif