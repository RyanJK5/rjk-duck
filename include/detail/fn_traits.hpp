#ifndef RJK_FN_TRAITS_HPP
#define RJK_FN_TRAITS_HPP

#include <type_traits>


namespace rjk {
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
}

#endif