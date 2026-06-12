#ifndef RJK_REMOVE_NOEXCEPT_HPP
#define RJK_REMOVE_NOEXCEPT_HPP

namespace rjk {
template <typename Func>
struct remove_noexcept_trait {
    using type = Func;
};

template <typename Ret, typename... Args>
struct remove_noexcept_trait<Ret(Args...) noexcept> {
    using type = Ret(Args...);
};

template <typename Ret, typename... Args>
struct remove_noexcept_trait<Ret(Args...) const noexcept> {
    using type = Ret(Args...) const;
};

template <typename Ret, typename... Args>
struct remove_noexcept_trait<Ret(Args...) & noexcept> {
    using type = Ret(Args...) &;
};

template <typename Ret, typename... Args>
struct remove_noexcept_trait<Ret(Args...) const & noexcept> {
    using type = Ret(Args...) const &;
};

template <typename Ret, typename... Args>
struct remove_noexcept_trait<Ret(Args...) && noexcept> {
    using type = Ret(Args...) &&;
};

template <typename Ret, typename... Args>
struct remove_noexcept_trait<Ret(Args...) const && noexcept> {
    using type = Ret(Args...) const &&;
};

template <typename T>
using remove_noexcept_t = remove_noexcept_trait<T>::type;

template <std::meta::info T>
using remove_noexcept_meta = remove_noexcept_t<typename [:T:]>;

consteval std::meta::info remove_noexcept(std::meta::info type) {
    return dealias(substitute(^^remove_noexcept_meta, {reflect_constant(type)}));
}
}

#endif