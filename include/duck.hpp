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

    template <typename... Policies>
    class duck :
        detail::make_duck_base_t<duck<Policies...>, Policies...>,
        public detail::make_duck_base_t<duck<Policies...>, Policies...>::vtable_wrapper {
      private:
        template <typename T>
        struct init_tag{};

        using duck_base_t = detail::make_duck_base_t<duck<Policies...>, Policies...>;
      public:
        constexpr duck() = default;

        template <typename T> requires (!std::same_as<std::decay_t<T>, duck> && duck_base_t::template meets_tags<T>())
        explicit duck(T&& obj)
            noexcept(std::is_nothrow_constructible_v<std::decay_t<T>, T> && detail::fits_sbo<std::decay_t<T>>)
            : duck(init_tag<std::decay_t<T>>{}, std::forward<T>(obj)) {
        }

        template <typename T, typename... Args> requires (duck_base_t::template meets_tags<T>())
        explicit duck(std::in_place_type_t<T>, Args&&... args)
            noexcept(std::is_nothrow_constructible_v<std::decay_t<T>, Args...> && detail::fits_sbo<std::decay_t<T>>)
            : duck(init_tag<std::decay_t<T>>{},std::forward<Args>(args)...) { }

        template <typename T, typename U, typename... Args> requires (duck_base_t::template meets_tags<T>())
        explicit duck(std::in_place_type_t<T>, std::initializer_list<U> il, Args&&... args)
            noexcept(std::is_nothrow_constructible_v<std::decay_t<T>, std::initializer_list<U>, Args...> && detail::fits_sbo<std::decay_t<T>>)
            : duck(init_tag<std::decay_t<T>>{}, il, std::forward<Args>(args)...) { }

        template <typename T> requires (duck_base_t::template meets_tags<T>())
        duck& operator=(T&& obj)
            noexcept(std::is_nothrow_constructible_v<std::decay_t<T>, T> && detail::fits_sbo<std::decay_t<T>>) {
            init_from<std::decay_t<T>>(std::forward<T>(obj));
            return *this;
        }

        template <std::size_t VtableIndex, detail::fn_qualifiers Qualifiers, typename Func>
        friend class duck_base_t::vtable_function;
      public:
        template <typename T, typename... Args> requires (duck_base_t::template meets_tags<T>())
        std::decay_t<T>& emplace(Args&&... args)
            noexcept(std::is_nothrow_constructible_v<std::decay_t<T>, Args&&...> && detail::fits_sbo<std::decay_t<T>>) {
            return *init_from<std::decay_t<T>>(std::forward<Args>(args)...);
        }

        template <typename T, typename U, typename... Args> requires (duck_base_t::template meets_tags<T>())
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

        template <typename T> requires (duck_base_t::template meets_tags<T>())
        T* get_if() noexcept {
            if (!m_underlying.template has_type<T>()) {
                return nullptr;
            }
            return static_cast<T*>(m_underlying.get());
        }

        template <typename T> requires (duck_base_t::template meets_tags<T>())
        const T* get_if() const noexcept {
            if (!m_underlying.template has_type<T>()) {
                return nullptr;
            }
            return static_cast<const T*>(m_underlying.get());
        }

        template <typename T> requires (duck_base_t::template meets_tags<T>())
        T& get() & {
            if (!m_underlying.template has_type<T>()) {
                throw bad_duck_access{};
            }
            return *static_cast<T*>(m_underlying.get());
        }

        template <typename T> requires (duck_base_t::template meets_tags<T>())
        const T& get() const & {
            if (!m_underlying.template has_type<T>()) {
                throw bad_duck_access{};
            }
            return *static_cast<const T*>(m_underlying.get());
        }

        template <typename T> requires (duck_base_t::template meets_tags<T>())
        T&& get() && {
            if (!m_underlying.template has_type<T>()) {
                throw bad_duck_access{};
            }
            return std::move(*static_cast<T*>(m_underlying.get()));
        }

        template <typename T> requires (duck_base_t::template meets_tags<T>())
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
        // rjk::duck generates most operators as friends via generate_operators.py,
        // since there is currently no way to easily reflect between std::meta::operators
        // and the actual operator functions.
        #include "generated/operator_friends.inl"

        // The following operators are special cases.

        // operator++/operator-- (postfix): explicitly use int as the second argument

        template <typename This>
        requires (duck_base_t::template satisfies_operator<op_plus_plus, This, void>(op_overload_kind::binary_lhs))
        friend decltype(auto) operator++(This&& operand, int) {
            return std::forward<This>(operand)._rjk__lhs_op_plus_plus(0);
        }

        template <typename This>
        requires (duck_base_t::template satisfies_operator<op_minus_minus, This, void>(op_overload_kind::binary_lhs))
        friend decltype(auto) operator--(This&& operand, int) {
            return std::forward<This>(operand)._rjk__lhs_op_minus_minus(0);
        }

      public:
        // operator-> / operator->*: has unique call syntax, must be defined as
        // member functions.

        template <typename This>
        requires (duck_base_t::template satisfies_operator<op_arrow, This, void>(op_overload_kind::unary))
        decltype(auto) operator->(this This&& operand) {
            return std::forward<This>(operand)._rjk__unary_op_arrow();
        }

        template <typename This, typename Rhs>
        requires (duck_base_t::template satisfies_operator<op_arrow_star, This, Rhs>(op_overload_kind::binary_lhs))
        decltype(auto) operator->*(this This&& lhs, Rhs&& rhs) {
            return std::forward<This>(lhs)._rjk__lhs_op_arrow_star(std::forward<Rhs>(rhs));
        }

        // operator() / operator[]: can have an arbitrary number of arguments,
        // must be defined as member functions

        template <typename This, typename... Args>
        requires (duck_base_t::template satisfies_operator<op_parentheses, This, void>(op_overload_kind::variadic))
        decltype(auto) operator()(this This&& operand, Args&&... args) {
            return std::forward<This>(operand)._rjk__op_parentheses(std::forward<Args>(args)...);
        }

        template <typename This, typename... Args>
        requires (duck_base_t::template satisfies_operator<op_square_brackets, This, void>(op_overload_kind::variadic))
        decltype(auto) operator[](this This&& operand, Args&&... args) {
            return std::forward<This>(operand)._rjk__op_square_brackets(std::forward<Args>(args)...);
        }
      private:
        detail::storage<duck_base_t> m_underlying{};
    };

namespace detail {

    template <typename Derived, duck_tag... Tags>
    template <std::size_t VtableIndex, fn_qualifiers Qualifiers, typename Ret, typename... Args>
    Derived& duck_base<Derived, Tags...>::vtable_function<VtableIndex, Qualifiers, Ret(Args...)>
    ::trace_to_duck() {
        auto* wrapper = reinterpret_cast<vtable_function_wrapper_t*>(this);
        return *static_cast<Derived*>(wrapper);
    }

    template <typename Derived, duck_tag... Tags>
    template <std::size_t VtableIndex, fn_qualifiers Qualifiers, typename Ret, typename... Args>
    const Derived& duck_base<Derived, Tags...>::vtable_function<VtableIndex, Qualifiers, Ret(Args...)>
    ::trace_to_duck() const {
        const auto* wrapper = reinterpret_cast<const vtable_function_wrapper_t*>(this);
        return *static_cast<const Derived*>(wrapper);
    }

    template <typename Derived, duck_tag... Tags>
    template <std::size_t VtableIndex, fn_qualifiers Qualifiers, typename Ret, typename... Args>
    Ret duck_base<Derived, Tags...>::vtable_function<VtableIndex, Qualifiers, Ret(Args...)>
    ::operator()(Args... args) requires (Qualifiers == fn_qualifiers::none) {
        auto& storage = trace_to_duck().m_underlying;
        return storage.vtable()->[:static_vtable_member:](
            storage.get(),
            std::forward<Args>(args)...
        );
    }

    template <typename Derived, duck_tag... Tags>
    template <std::size_t VtableIndex, fn_qualifiers Qualifiers, typename Ret, typename... Args>
    Ret duck_base<Derived, Tags...>::vtable_function<VtableIndex, Qualifiers, Ret(Args...)>
    ::operator()(Args... args) & requires (Qualifiers == fn_qualifiers::lvalue_ref) {
        auto& storage = trace_to_duck().m_underlying;
        return storage.vtable()->[:static_vtable_member:](
            storage.get(),
            std::forward<Args>(args)...
        );
    }

    template <typename Derived, duck_tag... Tags>
    template <std::size_t VtableIndex, fn_qualifiers Qualifiers, typename Ret, typename... Args>
    Ret duck_base<Derived, Tags...>::vtable_function<VtableIndex, Qualifiers, Ret(Args...)>
    ::operator()(Args... args) && requires (Qualifiers == fn_qualifiers::rvalue_ref) {
        auto& storage = trace_to_duck().m_underlying;
        return storage.vtable()->[:static_vtable_member:](
            storage.get(),
            std::forward<Args>(args)...
        );
    }

    template <typename Derived, duck_tag... Tags>
    template <std::size_t VtableIndex, fn_qualifiers Qualifiers, typename Ret, typename... Args>
    Ret duck_base<Derived, Tags...>::vtable_function<VtableIndex, Qualifiers, Ret(Args...)>
    ::operator()(Args... args) const requires (Qualifiers == fn_qualifiers::is_const) {
        const auto& storage = trace_to_duck().m_underlying;
        return storage.vtable()->[:static_vtable_member:](
            storage.get(),
            std::forward<Args>(args)...
        );
    }

    template <typename Derived, duck_tag... Tags>
    template <std::size_t VtableIndex, fn_qualifiers Qualifiers, typename Ret, typename... Args>
    Ret duck_base<Derived, Tags...>::vtable_function<VtableIndex, Qualifiers, Ret(Args...)>
    ::operator()(Args... args) const &
    requires (Qualifiers == (fn_qualifiers::is_const | fn_qualifiers::lvalue_ref)) {
        const auto& storage = trace_to_duck().m_underlying;
        return storage.vtable()->[:static_vtable_member:](
            storage.get(),
            std::forward<Args>(args)...
        );
    }

    template <typename Derived, duck_tag... Tags>
    template <std::size_t VtableIndex, fn_qualifiers Qualifiers, typename Ret, typename... Args>
    Ret duck_base<Derived, Tags...>::vtable_function<VtableIndex, Qualifiers, Ret(Args...)>
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