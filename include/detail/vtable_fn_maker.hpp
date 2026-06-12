#ifndef RJK_VTABLE_FN_MAKER_HPP
#define RJK_VTABLE_FN_MAKER_HPP

#include "generated/do_binary_op.hpp"
#include "generated/do_unary_op.hpp"
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

template <typename Sig, fn_qualifiers Qualifiers, std::meta::operators Op,
    op_overload_kind Kind, typename T>
struct vtable_op_maker;

template <typename Ret, typename... Args, fn_qualifiers Qualifiers, std::meta::operators Op,
    op_overload_kind Kind, typename T>
struct vtable_op_maker<Ret(Args...), Qualifiers, Op, Kind, T> {
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

            if constexpr (Op == op_parentheses) {
                return static_cast<ref_type>(*typed)
                    .operator()(std::forward<Args>(args)...);
            } else if constexpr (Op == op_square_brackets) {
                return static_cast<ref_type>(*typed)
                    .operator[](std::forward<Args>(args)...);
            } else if constexpr (Kind == op_overload_kind::unary) {
                return do_unary_op<Op>(static_cast<ref_type>(*typed));
            } else {
                if constexpr (Kind == op_overload_kind::binary_lhs) {
                    return do_binary_op<Op>(static_cast<ref_type>(*typed),
                        std::forward<Args...[0]>(args...[0]));
                }
                else {
                    return do_binary_op<Op>(std::forward<Args...[0]>(args...[0]),
                        static_cast<ref_type>(*typed));
                }
            }
        };
    }
};


// TODO: Remove once GCC fixes bug
template <std::meta::info Sig, fn_qualifiers Qualifiers, std::meta::operators Op,
    op_overload_kind Kind, std::meta::info T>
using vtable_op_maker_meta = vtable_op_maker<typename [:Sig:], Qualifiers, Op, Kind, typename [:T:]>;

// TODO: Remove once GCC fixes bug
template <std::meta::info Sig, fn_qualifiers Qualifiers, std::meta::info TMember, std::meta::info T>
using vtable_fn_maker_meta = vtable_fn_maker<typename [:Sig:], Qualifiers, TMember, typename [:T:]>;
}

#endif //RJK_DUCK_VTABLE_FN_MAKER_HPP
