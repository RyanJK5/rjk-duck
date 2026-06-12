#ifndef RJK_FN_TRAITS_HPP
#define RJK_FN_TRAITS_HPP

#include <type_traits>
#include <meta>
#include <ranges>

namespace rjk {

template <typename Ret, typename... Args>
using make_func_t = Ret(Args...);

consteval std::meta::info make_func(std::meta::info ret, std::meta::reflection_range auto&& args) {
    std::vector template_args{ret};
    template_args.append_range(args);
    return dealias(substitute(^^make_func_t, template_args));
}

template <typename T>
struct is_fn_const : std::false_type {};

template <typename Ret, typename... Args>
struct is_fn_const<Ret(Args...) const> : std::true_type {};

template <typename Ret, typename... Args>
struct is_fn_const<Ret(Args...) const &> : std::true_type {};

template <typename Ret, typename... Args>
struct is_fn_const<Ret(Args...) const &&> : std::true_type {};
}

namespace rjk {
template <typename T>
struct is_fn_lvalue_ref : std::false_type {};

template <typename Ret, typename... Args>
struct is_fn_lvalue_ref<Ret(Args...) &> : std::true_type {};

template <typename Ret, typename... Args>
struct is_fn_lvalue_ref<Ret(Args...) const &> : std::true_type {};
}

namespace rjk {
template <typename T>
struct is_fn_rvalue_ref : std::false_type {};

template <typename Ret, typename... Args>
struct is_fn_rvalue_ref<Ret(Args...) &&> : std::true_type {};

template <typename Ret, typename... Args>
struct is_fn_rvalue_ref<Ret(Args...) const &&> : std::true_type {};
}

namespace rjk {
template <typename Func>
struct decompose_fn_trait;

template <typename Ret, typename... Args>
struct decompose_fn_trait<Ret(Args...)> {
    using ret = Ret;
    using args = std::tuple<Args...>;
};

template <typename Func>
using fn_return_type_t = typename decompose_fn_trait<Func>::ret;

template <typename Func>
using fn_tuple_args_t = typename decompose_fn_trait<Func>::args;

template <typename Func, std::size_t Index>
using fn_arg_t = typename std::tuple_element<
    Index, typename decompose_fn_trait<Func>::args>::type;

template <typename Func>
constexpr auto fn_arg_count_v = std::tuple_size_v<typename decompose_fn_trait<
    Func>::args>;

template <typename Func>
consteval std::size_t count_args_of_type(std::meta::info searchType) {
    using Args = decompose_fn_trait<Func>::args;
    return std::ranges::count_if(template_arguments_of(dealias(^^Args)), [&](auto T) {
        return searchType == decay(T);
    });
}

namespace detail {
    template <typename F, typename RefType, typename Ret, typename... Args>
    concept callable_like = requires(F&& func, Args&&... args) {
        { static_cast<RefType>(func).operator()(std::forward<Args>(args)...)} -> std::same_as<Ret>;
    };

    template <typename T, typename RefType, typename Func>
    struct callable_like_func;

    template <typename T, typename RefType, typename Ret, typename... Args>
    struct callable_like_func<T, RefType, Ret(Args...)> {
        constexpr static bool value = callable_like<T, RefType, Ret, Args...>;
    };

    template <typename T, typename RefType, typename Func>
    constexpr static bool callable_like_func_v = callable_like_func<T, RefType, Func>::value;

    template <typename F, typename RefType, typename Ret, typename... Args>
    concept indexable = requires(F&& func, Args&&... args) {
        { static_cast<RefType>(func).operator[](std::forward<Args>(args)...) } -> std::same_as<Ret>;
    };

    template <typename T, typename RefType, typename Func>
    struct indexable_like_func;

    template <typename T, typename RefType, typename Ret, typename... Args>
    struct indexable_like_func<T, RefType, Ret(Args...)> {
        constexpr static bool value = indexable<T, RefType, Ret, Args...>;
    };

    template <typename T, typename RefType, typename Func>
    constexpr static bool indexable_like_func_v = indexable_like_func<T, RefType, Func>::value;
}
}

#endif