#ifndef RJK_VTABLE_FN_MAKER_HPP
#define RJK_VTABLE_FN_MAKER_HPP

#include <new>

#include "generated/do_binary_op.hpp"
#include "generated/do_unary_op.hpp"
#include "detail/remove_fn_qualifiers.hpp"
#include "duck_tags.hpp"
#include "overload_resolution.hpp"

namespace rjk::detail {

consteval static bool is_duck_container(std::meta::info type);

template <typename TraitRet, typename ActualRet>
consteval bool is_conversion_noexcept() {
    if constexpr (is_duck_container(^^TraitRet)) {
        return TraitRet::template nothrow_constructor<std::decay_t<ActualRet>, ActualRet>;
    } else {
        return true;
    }
};

consteval bool is_conversion_noexcept_type(std::meta::info trait_ret, std::meta::info actual_ret) {
    return extract<bool (*)()>(substitute(^^is_conversion_noexcept, {trait_ret, actual_ret}))();
}

// Return deduction implementation for duck.
template <typename TraitRet, typename ActualRet>
constexpr TraitRet
convert_duck_return(ActualRet&& result) noexcept(is_conversion_noexcept<TraitRet, ActualRet>()) {
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

// How a vtable entry reaches its object. A view's entries take the object's
// address; a duck's take its storage and know whether the object is buffered,
// so a call doesn't have to look that up.
struct address_access {
    template <bool IsConst>
    using erased_ptr = std::conditional_t<IsConst, const void*, void*>;

    // A duck's object may be in its buffer, so the address is laundered.
    template <typename Obj>
    constexpr static Obj* object(auto* address) noexcept {
        return std::launder(static_cast<Obj*>(address));
    }
};

template <typename Storage, bool Buffered>
struct storage_access {
    template <bool IsConst>
    using erased_ptr = std::conditional_t<IsConst, const Storage*, Storage*>;

    // Constant evaluation never uses the buffer.
    template <typename Obj>
    constexpr static Obj* object(auto* storage) noexcept {
        if !consteval {
            if constexpr (Buffered) {
                return std::launder(reinterpret_cast<Obj*>(storage->buffer.data()));
            }
        }
        return static_cast<Obj*>(storage->ptr);
    }
};

template <typename Sig, fn_qualifiers Qualifiers, typename T, typename Invoker, typename Access>
struct vtable_fn_maker;

template <typename Ret, typename... Args, bool Noexcept, fn_qualifiers Qualifiers, typename T,
          typename Invoker, typename Access>
struct vtable_fn_maker<Ret(Args...) noexcept(Noexcept), Qualifiers, T, Invoker, Access> {
    using erased_ptr_type =
        Access::template erased_ptr<static_cast<bool>(Qualifiers& fn_qualifiers::is_const)>;

    using function_ptr = Ret (*)(erased_ptr_type, Args...) noexcept(Noexcept);

    constexpr static Ret erased_call(erased_ptr_type context, Args... args) noexcept(Noexcept) {
        using obj_type =
            std::conditional_t<static_cast<bool>(Qualifiers & fn_qualifiers::is_const), const T, T>;
        using ref_type = std::conditional_t<static_cast<bool>(Qualifiers & fn_qualifiers::rvalue_ref),
                                            obj_type&&, obj_type&>;

        auto* typed = Access::template object<obj_type>(context);

        // We have to branch here so a void type doesn't get forwarded to
        // convert_duck_return.
        if constexpr (std::same_as<Ret, void>) {
            return Invoker{}(static_cast<ref_type>(*typed), std::forward<Args>(args)...);
        } else {
            return convert_duck_return<Ret>(
                Invoker{}(static_cast<ref_type>(*typed), std::forward<Args>(args)...));
        }
    }

    consteval static function_ptr make() { return &erased_call; }
};

template <typename Sig, fn_qualifiers Qualifiers, std::meta::operators Op, op_overload_kind Kind, typename T,
          typename Access>
struct vtable_op_maker;

template <typename Ret, typename... Args, bool Noexcept, fn_qualifiers Qualifiers, std::meta::operators Op,
          op_overload_kind Kind, typename T, typename Access>
struct vtable_op_maker<Ret(Args...) noexcept(Noexcept), Qualifiers, Op, Kind, T, Access> {
    using erased_ptr_type =
        Access::template erased_ptr<static_cast<bool>(Qualifiers& fn_qualifiers::is_const)>;

    using function_ptr = Ret (*)(erased_ptr_type, Args...) noexcept(Noexcept);

    constexpr static Ret erased_call(erased_ptr_type context, Args... args) noexcept(Noexcept) {
        using obj_type =
            std::conditional_t<static_cast<bool>(Qualifiers & fn_qualifiers::is_const), const T, T>;
        using ref_type = std::conditional_t<static_cast<bool>(Qualifiers & fn_qualifiers::rvalue_ref),
                                            obj_type&&, obj_type&>;

        auto* typed = Access::template object<obj_type>(context);

        if constexpr (Op == op_parentheses) {
            if constexpr (std::same_as<Ret, void>) {
                static_cast<ref_type>(*typed)(std::forward<Args>(args)...);
                return;
            } else {
                decltype(auto) result = static_cast<ref_type>(*typed)(std::forward<Args>(args)...);
                return convert_duck_return<Ret>(std::forward<decltype(result)>(result));
            }
        } else if constexpr (Op == op_square_brackets) {
            if constexpr (std::same_as<Ret, void>) {
                static_cast<ref_type>(*typed)[std::forward<Args>(args)...];
                return;
            } else {
                decltype(auto) result = static_cast<ref_type>(*typed)[std::forward<Args>(args)...];
                return convert_duck_return<Ret>(std::forward<decltype(result)>(result));
            }
        } else if constexpr (Kind == op_overload_kind::unary) {
            if constexpr (std::same_as<Ret, void>) {
                do_unary_op<Op, Noexcept>(static_cast<ref_type>(*typed));
                return;
            } else {
                decltype(auto) result = do_unary_op<Op, Noexcept>(static_cast<ref_type>(*typed));
                return convert_duck_return<Ret>(std::forward<decltype(result)>(result));
            }
        } else {
            if constexpr (Kind == op_overload_kind::binary_lhs) {
                if constexpr (std::same_as<Ret, void>) {
                    do_binary_op<Op, Noexcept>(static_cast<ref_type>(*typed),
                                               std::forward<Args...[0]>(args...[0]));
                    return;
                } else {
                    decltype(auto) result = do_binary_op<Op, Noexcept>(static_cast<ref_type>(*typed),
                                                                       std::forward<Args...[0]>(args...[0]));
                    return convert_duck_return<Ret>(std::forward<decltype(result)>(result));
                }
            } else {
                if constexpr (std::same_as<Ret, void>) {
                    do_binary_op<Op, Noexcept>(std::forward<Args...[0]>(args...[0]),
                                               static_cast<ref_type>(*typed));
                    return;
                } else {
                    decltype(auto) result = do_binary_op<Op, Noexcept>(std::forward<Args...[0]>(args...[0]),
                                                                       static_cast<ref_type>(*typed));
                    return convert_duck_return<Ret>(std::forward<decltype(result)>(result));
                }
            }
        }
    }

    consteval static function_ptr make() noexcept { return &erased_call; }
};
} // namespace rjk::detail

#endif
