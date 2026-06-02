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

template <typename T, typename From, typename To, bool PreserveRefQualifiers>
struct substitute_type {
    using type = std::conditional_t<
        std::is_same_v<std::remove_cvref_t<T>, From>,
        std::conditional_t<PreserveRefQualifiers,
                           std::conditional_t<std::is_rvalue_reference_v<T>,
                                              std::add_rvalue_reference_t<
                                                  std::conditional_t<
                                                      std::is_const_v<
                                                          std::remove_reference_t
                                                          <T> >, const To, To> >
                                              ,
                                              std::conditional_t<
                                                  std::is_lvalue_reference_v<T>,
                                                  std::add_lvalue_reference_t<
                                                      std::conditional_t<
                                                          std::is_const_v<
                                                              std::remove_reference_t
                                                              <T> >, const To,
                                                          To> >,
                                                  std::conditional_t<
                                                      std::is_const_v<T>, const
                                                      To, To>
                                              >
                           >,
                           To
        >,
        T
    >;
};

template <typename Func, typename From, typename To, bool PreserveRefQualifiers,
    bool FirstOnly>
struct substitute_fn_args_trait;

template <typename Ret, typename... Args, typename From, typename To, bool
          PreserveRefQualifiers>
struct substitute_fn_args_trait<Ret(Args...), From, To, PreserveRefQualifiers, false> {
    using type = substitute_type<Ret, From, To, PreserveRefQualifiers>::type(
        typename substitute_type<Args, From, To, PreserveRefQualifiers>::type...
        );
};

template <typename Ret, typename Arg1, typename... Args, typename From, typename To, bool
          PreserveRefQualifiers>
struct substitute_fn_args_trait<Ret(Arg1, Args...), From, To, PreserveRefQualifiers, true> {
public:
    using type = std::conditional_t<
        std::is_same_v<std::decay_t<Arg1>, From>,
        // Arg1 matches. Substitute it, leave rest untouched
        Ret(typename substitute_type<Arg1, From, To, PreserveRefQualifiers>::type, Args...),
        // Arg1 doesn't match. Recurse into tail
        prepend_arg_t<Arg1,
            typename substitute_fn_args_trait<Ret(Args...), From, To, PreserveRefQualifiers, true>::type
        >
    >;
};

template <typename Ret, typename From, typename To, bool PreserveRefQualifiers>
struct substitute_fn_args_trait<Ret(), From, To, PreserveRefQualifiers, true> {
    using type = Ret();
};

template <typename Func, typename From, typename To, bool PreserveRefQualifiers, bool FirstOnly>
using substitute_fn_args_t = substitute_fn_args_trait<
    Func, From, To, PreserveRefQualifiers, FirstOnly>::type;

enum struct [[=detail::flag_enum]] substitute_options {
    none = 0,
    preserve_ref_qualifiers = 1,
    first_only = 1 << 1
};

consteval static std::meta::info substitute_fn_args(
    std::meta::info func, std::meta::info from, std::meta::info to, substitute_options options = substitute_options::preserve_ref_qualifiers) {
    using enum substitute_options;

    return dealias(substitute(^^substitute_fn_args_t,
        {func, from, to,
            std::meta::reflect_constant((options & preserve_ref_qualifiers) != none),
            std::meta::reflect_constant((options & first_only) != none)
        }));
}
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