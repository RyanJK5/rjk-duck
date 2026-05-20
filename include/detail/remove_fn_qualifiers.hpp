#ifndef RJK_REMOVE_FN_QUALIFIERS_HPP
#define RJK_REMOVE_FN_QUALIFIERS_HPP

#include "bind_type_trait.hpp"
#include "flag_enum.hpp"

#include <meta>

namespace rjk {
template <typename T>
struct remove_fn_qualifiers_trait {
    using type = T;
};

template <typename Ret, typename... Args>
struct remove_fn_qualifiers_trait<Ret(Args...) const> {
    using type = Ret(Args...);
};

template <typename Ret, typename... Args>
struct remove_fn_qualifiers_trait<Ret(Args...) &> {
    using type = Ret(Args...);
};

template <typename Ret, typename... Args>
struct remove_fn_qualifiers_trait<Ret(Args...) const &> {
    using type = Ret(Args...);
};

template <typename Ret, typename... Args>
struct remove_fn_qualifiers_trait<Ret(Args...) &&> {
    using type = Ret(Args...);
};

template <typename Ret, typename... Args>
struct remove_fn_qualifiers_trait<Ret(Args...) const &&> {
    using type = Ret(Args...);
};

template <typename Ret, typename... Args>
struct remove_fn_qualifiers_trait<Ret(Args...) const noexcept> {
    using type = Ret(Args...);
};

template <typename Ret, typename... Args>
struct remove_fn_qualifiers_trait<Ret(Args...) & noexcept> {
    using type = Ret(Args...);
};

template <typename Ret, typename... Args>
struct remove_fn_qualifiers_trait<Ret(Args...) const & noexcept> {
    using type = Ret(Args...);
};

template <typename Ret, typename... Args>
struct remove_fn_qualifiers_trait<Ret(Args...) && noexcept> {
    using type = Ret(Args...);
};

template <typename Ret, typename... Args>
struct remove_fn_qualifiers_trait<Ret(Args...) const && noexcept> {
    using type = Ret(Args...);
};

template <typename T>
using remove_fn_qualifiers_t = remove_fn_qualifiers_trait<T>::type;

consteval std::meta::info remove_fn_qualifiers(std::meta::info type) {
    return bind_type_trait<remove_fn_qualifiers_t>(type);
}

namespace detail {

enum struct [[=flag_enum]] fn_qualifiers {
    none = 0,
    is_const = 1,
    lvalue_ref = 1 << 1,
    rvalue_ref = 1 << 2
};

consteval fn_qualifiers qualifiers_of(std::meta::info function) {
    return
        (is_const(function) ? fn_qualifiers::is_const : fn_qualifiers::none) |
        (is_lvalue_reference_qualified(function)
             ? fn_qualifiers::lvalue_ref
             : fn_qualifiers::none) |
        (is_rvalue_reference_qualified(function)
             ? fn_qualifiers::rvalue_ref
             : fn_qualifiers::none);
}

template <typename Func, typename Self>
struct fn_arg_qualifiers_trait {
    constexpr static fn_qualifiers value = fn_qualifiers::none;
};

template <typename Ret, typename Arg, typename... Rest, typename Self>
struct fn_arg_qualifiers_trait<Ret(Arg, Rest...), Self> {
private:
    constexpr static bool this_is_self = std::is_same_v<
        std::remove_cvref_t<Arg>, Self>;

    constexpr static fn_qualifiers self_qualifiers =
        (std::is_reference_v<Arg> && std::is_const_v<std::remove_reference_t<
             Arg> >
             ? fn_qualifiers::is_const
             : fn_qualifiers::none) |
        (std::is_lvalue_reference_v<Arg> && !std::is_const_v<
             std::remove_reference_t<Arg> >
             ? fn_qualifiers::lvalue_ref
             : fn_qualifiers::none) |
        (std::is_rvalue_reference_v<Arg>
             ? fn_qualifiers::rvalue_ref
             : fn_qualifiers::none);

public:
    constexpr static fn_qualifiers value = this_is_self
                                               ? self_qualifiers
                                               : fn_arg_qualifiers_trait<
                                                   Ret(Rest...), Self>::value;
};

template <typename Func, typename Self>
constexpr fn_qualifiers fn_arg_qualifiers_v = fn_arg_qualifiers_trait<
    Func, Self>::value;

consteval fn_qualifiers qualifiers_of_target(std::meta::info functionType,
                                             std::meta::info target) {
    return extract<fn_qualifiers>(
        substitute(^^fn_arg_qualifiers_v, {functionType, target}));
}
}
}

#endif