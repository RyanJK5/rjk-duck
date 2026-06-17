// clang-format off

#ifndef RJK_REMOVE_FN_QUALIFIERS_HPP
#define RJK_REMOVE_FN_QUALIFIERS_HPP

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
struct remove_fn_qualifiers_trait<Ret(Args...) noexcept> {
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

// TODO: remove once GCC fixes reference return type bug
template <std::meta::info T>
using remove_fn_qualifiers_meta = remove_fn_qualifiers_t<typename [:T:]>;

// Removes const, reference qualifiers, and noexcept.
consteval std::meta::info remove_fn_qualifiers(std::meta::info type) {
    return dealias(substitute(^^remove_fn_qualifiers_meta, {reflect_constant(type)}));
}

namespace detail {

enum struct [[=flag_enum]] fn_qualifiers {
    none = 0,
    is_const = 1,
    lvalue_ref = 1 << 1,
    rvalue_ref = 1 << 2
};

// Checks the qualifiers of a type.
consteval fn_qualifiers qualifiers_of_type(std::meta::info type) {
    auto qualifiers = fn_qualifiers::none;
    if (is_const(remove_reference(type))) {
        qualifiers |= fn_qualifiers::is_const;
    }
    if (is_lvalue_reference_type(type) &&
        !static_cast<bool>(qualifiers & fn_qualifiers::is_const)) {
        qualifiers |= fn_qualifiers::lvalue_ref;
    }
    if (is_rvalue_reference_type(type)) {
        qualifiers |= fn_qualifiers::rvalue_ref;
    }
    return qualifiers;
}

// Checks the qualifiers of a function.
consteval fn_qualifiers qualifiers_of(std::meta::info function) {
    auto qualifiers = fn_qualifiers::none;
    if (is_const(function)) {
        qualifiers |= fn_qualifiers::is_const;
    }
    if (is_lvalue_reference_qualified(function)) {
        qualifiers |= fn_qualifiers::lvalue_ref;
    }
    if (is_rvalue_reference_qualified(function)) {
        qualifiers |= fn_qualifiers::rvalue_ref;
    }
    return qualifiers;
}

// Searches the parameter list for a parameter with the type of target, then
// returns the qualifiers of that type.
consteval fn_qualifiers qualifiers_of_target(std::meta::info functionType,
                                             std::meta::info target) {
    const auto params = parameters_of(functionType);
    const auto itr = std::ranges::find_if(params,
        [target](auto param) { return decay(param) == target; }
    );
    return qualifiers_of_type(*itr);
}

}
}

#endif