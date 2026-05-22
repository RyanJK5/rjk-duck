#ifndef RJK_VTABLE_FN_MAKER_HPP
#define RJK_VTABLE_FN_MAKER_HPP

#include "detail/remove_fn_qualifiers.hpp"

namespace rjk::detail {
template <typename Sig, fn_qualifiers Qualifiers, std::meta::info TMember, typename T>
struct vtable_fn_maker;

template <typename Ret, typename... Args, fn_qualifiers Qualifiers, std::meta::info TMember, typename T>
struct vtable_fn_maker<Ret(Args...), Qualifiers, TMember, T> {
    constexpr static auto refl_erased_ptr_type =
        static_cast<bool>(Qualifiers & fn_qualifiers::is_const)
        ? ^^const void* : ^^void*;
    using erased_ptr_type = [:refl_erased_ptr_type:];

    using function_ptr = Ret(*)(erased_ptr_type, Args...);

    consteval static function_ptr make() noexcept {
        return [](erased_ptr_type context, Args... args) -> Ret {
            using obj_type = std::conditional_t<
                static_cast<bool>(Qualifiers & fn_qualifiers::is_const), const T, T>;
            using ref_type = std::conditional_t<
                static_cast<bool>(Qualifiers & fn_qualifiers::rvalue_ref), obj_type&&, obj_type&>;

            return static_cast<ref_type>(*static_cast<obj_type*>(context))
                .[:TMember:](std::forward<Args>(args)...);
        };
    }
};

template <typename Sig, fn_qualifiers Qualifiers, std::meta::operators Op, bool SelfIsLhs, typename T>
struct vtable_op_maker;

template <typename Ret, typename... Args, fn_qualifiers Qualifiers, std::meta::operators Op, bool SelfIsLhs, typename T>
struct vtable_op_maker<Ret(Args...), Qualifiers, Op, SelfIsLhs, T> {
    constexpr static auto refl_erased_ptr_type =
        static_cast<bool>(Qualifiers & fn_qualifiers::is_const)
        ? ^^const void* : ^^void*;
    using erased_ptr_type = [:refl_erased_ptr_type:];

    using function_ptr = Ret(*)(erased_ptr_type, Args...);

    consteval static function_ptr make() noexcept {
        return [](erased_ptr_type context, Args... args) -> Ret {
            using obj_type = std::conditional_t<
                static_cast<bool>(Qualifiers & fn_qualifiers::is_const), const T, T>;
            using ref_type = std::conditional_t<
                static_cast<bool>(Qualifiers & fn_qualifiers::rvalue_ref), obj_type&&, obj_type&>;

            auto* typed = static_cast<obj_type*>(context);

            if constexpr (sizeof...(Args) == 0UZ) {
                if constexpr (Op == rjk::op_plus) return +static_cast<ref_type>(*typed);
            }
            if constexpr (Op == rjk::op_plus) {
                decltype(auto) arg1 = std::get<0>(std::forward_as_tuple(std::forward<Args>(args)...));
                if constexpr (SelfIsLhs) {
                    return static_cast<ref_type>(*typed) + arg1;
                } else {
                    return arg1 + static_cast<ref_type>(*typed);
                }
            }
        };
    }
};
}

#endif //RJK_DUCK_VTABLE_FN_MAKER_HPP
