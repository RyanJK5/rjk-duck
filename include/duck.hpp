// (No support for formatting reflection currently)
// clang-format off

#ifndef RJK_DUCK_HPP
#define RJK_DUCK_HPP

#include "duck.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <concepts>
#include <meta>

#include "duck_tags.hpp"
#include "detail/storage.hpp"

namespace rjk {
    struct bad_duck_access : std::exception {
        const char* what() const noexcept override {
            return "type mismatch";
        }
    };

    template <rjk::duck_tag... Tags>
    class duck : detail::duck_base<Tags...>, public detail::duck_base<Tags...>::vtable_wrapper {
      private:
        template <typename T>
        struct init_tag{};
      public:
        constexpr duck() = default;

        template <typename T> requires (!std::same_as<std::decay_t<T>, duck> && satisfies_tags<std::decay_t<T>, duck, Tags...>())
        explicit duck(T&& obj)
            noexcept(std::is_nothrow_constructible_v<std::decay_t<T>, T> && detail::fits_sbo<std::decay_t<T>>)
            : duck(init_tag<std::decay_t<T>>{}, std::forward<T>(obj)) {
        }

        template <typename T, typename... Args> requires (satisfies_tags<std::decay_t<T>, duck, Tags...>())
        explicit duck(std::in_place_type_t<T>, Args&&... args)
            noexcept(std::is_nothrow_constructible_v<std::decay_t<T>, Args...> && detail::fits_sbo<std::decay_t<T>>)
            : duck(init_tag<std::decay_t<T>>{},std::forward<Args>(args)...) { }

        template <typename T, typename U, typename... Args> requires (satisfies_tags<std::decay_t<T>, duck, Tags...>())
        explicit duck(std::in_place_type_t<T>, std::initializer_list<U> il, Args&&... args)
            noexcept(std::is_nothrow_constructible_v<std::decay_t<T>, std::initializer_list<U>, Args...> && detail::fits_sbo<std::decay_t<T>>)
            : duck(init_tag<std::decay_t<T>>{}, il, std::forward<Args>(args)...) { }

        template <typename T> requires (satisfies_tags<std::decay_t<T>, duck, Tags...>())
        duck& operator=(T&& obj)
            noexcept(std::is_nothrow_constructible_v<std::decay_t<T>, T> && detail::fits_sbo<std::decay_t<T>>) {
            init_from<std::decay_t<T>>(std::forward<T>(obj));
            return *this;
        }

        template <std::size_t VtableIndex, detail::fn_qualifiers Qualifiers, typename Func>
        friend class detail::duck_base<Tags...>::vtable_function;
      public:
        template <typename T, typename... Args> requires (satisfies_tags<T, duck, Tags...>())
        std::decay_t<T>& emplace(Args&&... args)
            noexcept(std::is_nothrow_constructible_v<std::decay_t<T>, Args&&...> && detail::fits_sbo<std::decay_t<T>>) {
            return *init_from<std::decay_t<T>>(std::forward<Args>(args)...);
        }

        template <typename T, typename U, typename... Args> requires (satisfies_tags<T, duck, Tags...>())
        std::decay_t<T>& emplace(std::initializer_list<U> il, Args&&... args)
            noexcept(std::is_nothrow_constructible_v<std::decay_t<T>, std::initializer_list<U>, Args&&...> && detail::fits_sbo<std::decay_t<T>>) {
            return *init_from<std::decay_t<T>>(il, std::forward<Args>(args)...);
        }

        void reset() noexcept {
            m_underlying.reset();
        }
      public:
        bool has_value() const noexcept {
            return m_underlying.has_value();
        }

        template <typename T> requires (satisfies_tags<T, duck, Tags...>())
        T* get_if() noexcept {
            if (!m_underlying.template has_type<T>()) {
                return nullptr;
            }
            return static_cast<T*>(m_underlying.get());
        }

        template <typename T> requires (satisfies_tags<T, duck, Tags...>())
        const T* get_if() const noexcept {
            if (!m_underlying.template has_type<T>()) {
                return nullptr;
            }
            return static_cast<const T*>(m_underlying.get());
        }

        template <typename T> requires (satisfies_tags<T, duck, Tags...>())
        T& get() & {
            if (!m_underlying.template has_type<T>()) {
                throw bad_duck_access{};
            }
            return *static_cast<T*>(m_underlying.get());
        }

        template <typename T> requires (satisfies_tags<T, duck, Tags...>())
        const T& get() const & {
            if (!m_underlying.template has_type<T>()) {
                throw bad_duck_access{};
            }
            return *static_cast<const T*>(m_underlying.get());
        }

        template <typename T> requires (satisfies_tags<T, duck, Tags...>())
        T&& get() && {
            if (!m_underlying.template has_type<T>()) {
                throw bad_duck_access{};
            }
            return std::move(*static_cast<T*>(m_underlying.get()));
        }

        template <typename T> requires (satisfies_tags<T, duck, Tags...>())
        const T&& get() const && {
            if (!m_underlying.template has_type<T>()) {
                throw bad_duck_access{};
            }
            return std::move(*static_cast<const T*>(m_underlying.get()));
        }
      private:
        template <typename T, typename... Args>
        std::decay_t<T>* init_from(Args&&... args) noexcept(std::is_nothrow_constructible_v<std::decay_t<T>, Args&&...> && detail::fits_sbo<std::decay_t<T>>) {
            m_underlying.template emplace<T>(std::forward<Args>(args)...);
            return static_cast<std::decay_t<T>*>(m_underlying.get());
        }

        template <typename T, typename... Args>
        duck(init_tag<T>, Args&&... args) noexcept(std::is_nothrow_constructible_v<std::decay_t<T>, Args&&...> && detail::fits_sbo<std::decay_t<T>>)
            : m_underlying(std::in_place_type<T>, std::forward<Args>(args)...)
        { }
      private:
        template <std::meta::operators Op, typename Lhs, typename Rhs>
        consteval static bool satisfies_operator(op_overload_kind kind) noexcept {
            if (!has_operator_tag<Op, Tags...>(kind)) {
                return false;
            }

            switch (kind) {
                using enum op_overload_kind;
            case any_kind:
                throw std::logic_error("Must use specific kind");
            case binary_lhs:
                return std::same_as<std::decay_t<Lhs>, duck>;
            case binary_rhs:
                return std::same_as<std::decay_t<Rhs>, duck> && !std::same_as<std::decay_t<Lhs>, std::decay_t<Rhs>>;
            case unary:
                return std::same_as<std::decay_t<Lhs>, duck>;
            }
            return false;
        }

        template <typename This> requires (satisfies_operator<op_plus, This, void>(op_overload_kind::unary))
        friend decltype(auto) operator+(This&& operand) {
            return std::forward<This>(operand)._rjk__unary_op_plus();
        }

        template <typename This, typename R> requires (satisfies_operator<op_plus, This, R>(op_overload_kind::binary_lhs))
        friend decltype(auto) operator+(This&& lhs, R&& rhs) {
            return std::forward<This>(lhs)._rjk__lhs_op_plus(std::forward<R>(rhs));
        }

        template <typename L, typename This> requires (satisfies_operator<op_plus, L, This>(op_overload_kind::binary_rhs))
        friend decltype(auto) operator+(L&& lhs, This&& rhs) {
            return std::forward<This>(rhs)._rjk__rhs_op_plus(std::forward<L>(lhs));
        }
      private:
        detail::storage<Tags...> m_underlying{};
    };

namespace detail {

    template <duck_tag... Tags>
    template <std::size_t VtableIndex, fn_qualifiers Qualifiers, typename Ret, typename... Args>
    duck<Tags...>& duck_base<Tags...>::vtable_function<VtableIndex, Qualifiers, Ret(Args...)>
    ::trace_to_duck() {
        auto* wrapper = reinterpret_cast<vtable_function_wrapper_t*>(this);
        return *static_cast<duck<Tags...>*>(wrapper);
    }

    template <duck_tag... Tags>
    template <std::size_t VtableIndex, fn_qualifiers Qualifiers, typename Ret, typename... Args>
    const duck<Tags...>& duck_base<Tags...>::vtable_function<VtableIndex, Qualifiers, Ret(Args...)>
    ::trace_to_duck() const {
        const auto* wrapper = reinterpret_cast<const vtable_function_wrapper_t*>(this);
        return *static_cast<const duck<Tags...>*>(wrapper);
    }

    template <duck_tag... Tags>
    template <std::size_t VtableIndex, fn_qualifiers Qualifiers, typename Ret, typename... Args>
    Ret duck_base<Tags...>::vtable_function<VtableIndex, Qualifiers, Ret(Args...)>
    ::operator()(Args... args) requires (Qualifiers == fn_qualifiers::none) {
        auto& storage = trace_to_duck().m_underlying;
        return storage.vtable()->[:static_vtable_member:](
            storage.get(),
            std::forward<Args>(args)...
        );
    }

    template <duck_tag... Tags>
    template <std::size_t VtableIndex, fn_qualifiers Qualifiers, typename Ret, typename... Args>
    Ret duck_base<Tags...>::vtable_function<VtableIndex, Qualifiers, Ret(Args...)>
    ::operator()(Args... args) & requires (Qualifiers == fn_qualifiers::lvalue_ref) {
        auto& storage = trace_to_duck().m_underlying;
        return storage.vtable()->[:static_vtable_member:](
            storage.get(),
            std::forward<Args>(args)...
        );
    }

    template <duck_tag... Tags>
    template <std::size_t VtableIndex, fn_qualifiers Qualifiers, typename Ret, typename... Args>
    Ret duck_base<Tags...>::vtable_function<VtableIndex, Qualifiers, Ret(Args...)>
    ::operator()(Args... args) && requires (Qualifiers == fn_qualifiers::rvalue_ref) {
        auto& storage = trace_to_duck().m_underlying;
        return storage.vtable()->[:static_vtable_member:](
            storage.get(),
            std::forward<Args>(args)...
        );
    }

    template <duck_tag... Tags>
    template <std::size_t VtableIndex, fn_qualifiers Qualifiers, typename Ret, typename... Args>
    Ret duck_base<Tags...>::vtable_function<VtableIndex, Qualifiers, Ret(Args...)>
    ::operator()(Args... args) const requires (Qualifiers == fn_qualifiers::is_const) {
        const auto& storage = trace_to_duck().m_underlying;
        return storage.vtable()->[:static_vtable_member:](
            storage.get(),
            std::forward<Args>(args)...
        );
    }

    template <duck_tag... Tags>
    template <std::size_t VtableIndex, fn_qualifiers Qualifiers, typename Ret, typename... Args>
    Ret duck_base<Tags...>::vtable_function<VtableIndex, Qualifiers, Ret(Args...)>
    ::operator()(Args... args) const &
    requires (Qualifiers == (fn_qualifiers::is_const | fn_qualifiers::lvalue_ref)) {
        const auto& storage = trace_to_duck().m_underlying;
        return storage.vtable()->[:static_vtable_member:](
            storage.get(),
            std::forward<Args>(args)...
        );
    }

    template <duck_tag... Tags>
    template <std::size_t VtableIndex, fn_qualifiers Qualifiers, typename Ret, typename... Args>
    Ret duck_base<Tags...>::vtable_function<VtableIndex, Qualifiers, Ret(Args...)>
    ::operator()(Args... args) const && requires (Qualifiers == (fn_qualifiers::is_const | fn_qualifiers::rvalue_ref)) {
        const auto& storage = trace_to_duck().m_underlying;
        return storage.vtable()->[:static_vtable_member:](
            storage.get(),
            std::forward<Args>(args)...
        );
    }
}
}

#endif