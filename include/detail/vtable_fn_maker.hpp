// clang-format off

#ifndef RJK_VTABLE_FN_MAKER_HPP
#define RJK_VTABLE_FN_MAKER_HPP

#include "generated/do_binary_op.hpp"
#include "generated/do_unary_op.hpp"
#include "detail/remove_fn_qualifiers.hpp"

namespace rjk::detail {

// Return deduction implementation for duck.
template <typename TraitRet, typename ActualRet>
constexpr TraitRet convert_duck_return(ActualRet&& result) {
    if constexpr (std::same_as<TraitRet, ActualRet>) {
        return std::forward<ActualRet>(result);
    } else if constexpr (is_pointer_type(^^ActualRet)) {
        if (result == nullptr) {
            return TraitRet{};
        }
        return TraitRet{*result};
    } else {
        return TraitRet{std::forward<ActualRet>(result)};
    }
}

template <typename Sig, fn_qualifiers Qualifiers, std::meta::info TMember, typename T, bool FunctionCallSyntax>
struct vtable_fn_maker;

template <typename Ret, typename... Args, fn_qualifiers Qualifiers, std::meta::info TMember, typename T, bool FunctionCallSyntax>
struct vtable_fn_maker<Ret(Args...), Qualifiers, TMember, T, FunctionCallSyntax> {
    constexpr static auto refl_erased_ptr_type =
        static_cast<bool>(Qualifiers & fn_qualifiers::is_const)
        ? ^^const void* : ^^void*;
    using erased_ptr_type = [:refl_erased_ptr_type:];

    using function_ptr = Ret(*)(erased_ptr_type, Args...);

    constexpr static Ret erased_call(erased_ptr_type context, Args... args) {
        using obj_type = std::conditional_t<
                static_cast<bool>(Qualifiers & fn_qualifiers::is_const), const T, T>;
        using ref_type = std::conditional_t<
            static_cast<bool>(Qualifiers & fn_qualifiers::rvalue_ref), obj_type&&, obj_type&>;

        // We have to branch here so a void type doesn't get forwarded to
        // convert_duck_return.
        if constexpr (std::same_as<Ret, void>) {
            if constexpr (FunctionCallSyntax) {
                return [:TMember:](
                    static_cast<ref_type>(*static_cast<obj_type*>(context)),
                    std::forward<Args>(args)...
                );
            } else {
                return static_cast<ref_type>(*static_cast<obj_type*>(context))
                    .[:TMember:](std::forward<Args>(args)...);
            }
        } else {
            if constexpr (FunctionCallSyntax) {
                decltype(auto) result = [:TMember:](
                    static_cast<ref_type>(*static_cast<obj_type*>(context)),
                    std::forward<Args>(args)...
                );
                return convert_duck_return<Ret>(std::forward<decltype(result)>(result));
            } else {
                decltype(auto) result = static_cast<ref_type>(*static_cast<obj_type*>(context))
                    .[:TMember:](std::forward<Args>(args)...);
                return convert_duck_return<Ret>(std::forward<decltype(result)>(result));
            }
        }
    }

    consteval static function_ptr make() {
        return &erased_call;
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

    constexpr static Ret erased_call(erased_ptr_type context, Args... args) {
        using obj_type = std::conditional_t<
            static_cast<bool>(Qualifiers & fn_qualifiers::is_const), const T, T>;
        using ref_type = std::conditional_t<
            static_cast<bool>(Qualifiers & fn_qualifiers::rvalue_ref), obj_type&&, obj_type&>;

        auto* typed = static_cast<obj_type*>(context);

        if constexpr (Op == op_parentheses) {
            if constexpr (std::same_as<Ret, void>) {
                static_cast<ref_type>(*typed)
                    .operator()(std::forward<Args>(args)...);
                return;
            } else {
                decltype(auto) result = static_cast<ref_type>(*typed)
                    .operator()(std::forward<Args>(args)...);
                return convert_duck_return<Ret>(std::forward<decltype(result)>(result));
            }
        } else if constexpr (Op == op_square_brackets) {
            if constexpr (std::same_as<Ret, void>) {
                static_cast<ref_type>(*typed)
                    .operator[](std::forward<Args>(args)...);
                return;
            } else {
                decltype(auto) result = static_cast<ref_type>(*typed)
                    .operator[](std::forward<Args>(args)...);
                return convert_duck_return<Ret>(std::forward<decltype(result)>(result));
            }
        } else if constexpr (Kind == op_overload_kind::unary) {
            if constexpr (std::same_as<Ret, void>) {
                do_unary_op<Op>(static_cast<ref_type>(*typed));
                return;
            } else {
                decltype(auto) result =
                    do_unary_op<Op>(static_cast<ref_type>(*typed));
                return convert_duck_return<Ret>(std::forward<decltype(result)>(result));
            }
        } else {
            if constexpr (Kind == op_overload_kind::binary_lhs) {
                if constexpr (std::same_as<Ret, void>) {
                    do_binary_op<Op>(static_cast<ref_type>(*typed),
                        std::forward<Args...[0]>(args...[0]));
                    return;
                } else {
                    decltype(auto) result =
                        do_binary_op<Op>(static_cast<ref_type>(*typed),
                        std::forward<Args...[0]>(args...[0]));
                    return convert_duck_return<Ret>(std::forward<decltype(result)>(result));
                }
            }
            else {
                if constexpr (std::same_as<Ret, void>) {
                    do_binary_op<Op>(std::forward<Args...[0]>(args...[0]),
                        static_cast<ref_type>(*typed));
                    return;
                } else {
                    decltype(auto) result =
                        do_binary_op<Op>(std::forward<Args...[0]>(args...[0]),
                        static_cast<ref_type>(*typed));
                    return convert_duck_return<Ret>(std::forward<decltype(result)>(result));
                }
            }
        }
    }

    consteval static function_ptr make() noexcept {
        return &erased_call;
    }
};


// TODO: Remove once GCC fixes bug
template <std::meta::info Sig, fn_qualifiers Qualifiers, std::meta::operators Op,
    op_overload_kind Kind, std::meta::info T>
using vtable_op_maker_meta = vtable_op_maker<typename [:Sig:], Qualifiers, Op, Kind, typename [:T:]>;

// TODO: Remove once GCC fixes bug
template <std::meta::info Sig, fn_qualifiers Qualifiers, std::meta::info TMember, std::meta::info T>
using vtable_fn_maker_meta = vtable_fn_maker<
    typename [:Sig:], Qualifiers, TMember, typename [:T:], false>;

// TODO: Remove once GCC fixes bug
template <std::meta::info Sig, fn_qualifiers Qualifiers, std::meta::info TMember, std::meta::info T>
using vtable_fn_maker_for_impl_meta = vtable_fn_maker<
    typename [:Sig:], Qualifiers, TMember, typename [:T:], true>;
}

#endif //RJK_DUCK_VTABLE_FN_MAKER_HPP
