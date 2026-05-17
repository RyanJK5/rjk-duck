#pragma once

#include <meta>
#include <type_traits>

namespace rjk::detail {
    template <typename T, typename From, typename To, bool PreserveRefQualifiers>
    struct substitute_type {
        using type = std::conditional_t<
            std::is_same_v<std::remove_cvref_t<T>, From>,
            std::conditional_t<PreserveRefQualifiers,
                std::conditional_t<std::is_rvalue_reference_v<T>,
                    std::add_rvalue_reference_t<std::conditional_t<std::is_const_v<std::remove_reference_t<T>>, const To, To>>,
                    std::conditional_t<std::is_lvalue_reference_v<T>,
                        std::add_lvalue_reference_t<std::conditional_t<std::is_const_v<std::remove_reference_t<T>>, const To, To>>,
                        std::conditional_t<std::is_const_v<T>, const To, To>
                    >
                >,
                To
            >,
            T
        >;
    };

    template <typename Func, typename From, typename To, bool PreserveRefQualifiers>
    struct substitute_fn_args_trait;

    template <typename Ret, typename... Args, typename From, typename To, bool PreserveRefQualifiers>
    struct substitute_fn_args_trait<Ret(Args...), From, To, PreserveRefQualifiers> {
        using type = substitute_type<Ret, From, To, PreserveRefQualifiers>::type(
            typename substitute_type<Args, From, To, PreserveRefQualifiers>::type...
        );
    };

    template <typename Func, typename From, typename To, bool PreserveRefQualifiers>
    using substitute_fn_args_t = substitute_fn_args_trait<Func, From, To, PreserveRefQualifiers>::type;
    
    consteval static std::meta::info substitute_fn_args(std::meta::info func, std::meta::info from, std::meta::info to, const bool preserve_ref_qualifiers = true) {
        const auto preserve = std::meta::reflect_constant(preserve_ref_qualifiers);
        return dealias(substitute(^^substitute_fn_args_t, {func, from, to, preserve}));
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

    consteval std::meta::info remove_arg(std::meta::info func, std::meta::info to_remove) {
        return dealias(substitute(^^fn_remove_arg_t, {func, to_remove}));
    }
}