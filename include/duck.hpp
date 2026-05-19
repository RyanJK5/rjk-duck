// (No support for formatting reflection currently)
// clang-format off

#ifndef RJK_DUCK_HPP
#define RJK_DUCK_HPP

#include <algorithm>
#include <array>
#include <cassert>
#include <concepts>
#include <functional>
#include <meta>
#include <new>
#include <print>
#include <ranges>
#include <string_view>

#include "duck_tags.hpp"
#include "detail/substitute_fn_traits.hpp"
#include "detail/storage.hpp"

namespace rjk {
    template <rjk::duck_tag... Tags>
    class duck;
namespace detail {

    template <rjk::duck_tag... Tags>
    class duck_base {
      protected:
        struct vtable;

        template <typename Func, fn_qualifiers Qualifiers>
        class vtable_function;

        template <fn_qualifiers Qualifiers, typename Ret, typename... Args>
        class vtable_function<Ret(Args...), Qualifiers> {
          public:
            using function_type = Ret(*)(void*, Args...);

            void* obj = nullptr;
            function_type function = nullptr;

            vtable_function(void* obj, function_type func) noexcept
                : obj(obj), function(func)
            { }

            template <typename T, std::meta::info Member>
            static vtable_function make_fn(void* underlying) noexcept {
                return {
                    underlying,
                    [](void* context, Args... args) -> Ret {
                        using obj_type = std::conditional_t<
                            static_cast<bool>(Qualifiers & fn_qualifiers::is_const),
                            const T,
                            T
                        >;
                        using ref_type = std::conditional_t<
                            static_cast<bool>(Qualifiers & fn_qualifiers::rvalue_ref),
                            obj_type&&,
                            obj_type&
                        >;
                        auto* typed = static_cast<obj_type*>(context);
                        return static_cast<ref_type>(*typed).[:Member:](std::forward<Args>(args)...);
                    }
                };
            }

            template <typename RefType, bool HasOther, typename Arg>
            static decltype(auto) resolve_operator_arg(Arg&& arg) noexcept {
                if constexpr(HasOther) {
                    return static_cast<RefType>(*static_cast<std::remove_reference_t<RefType>*>(arg));
                }
                else {
                    return std::forward<Arg>(arg);
                }
            }

            template <typename T, std::meta::operators Op, bool SelfIsLhs>
            static vtable_function make_op(void* underlying) noexcept {
                return {
                    underlying,
                    [](void* context, Args... args) -> Ret {
                        using obj_type = std::conditional_t<
                            static_cast<bool>(Qualifiers & fn_qualifiers::is_const),
                            const T,
                            T
                        >;
                        using ref_type = std::conditional_t<
                            static_cast<bool>(Qualifiers & fn_qualifiers::rvalue_ref),
                            obj_type&&,
                            obj_type&
                        >;

                        auto* typed = static_cast<obj_type*>(context);

                        if constexpr(sizeof...(Args) == 0UZ) {
                            if constexpr(Op == rjk::op_plus) return +static_cast<ref_type>(*typed);
                        }
                        if constexpr(Op == rjk::op_plus) {
                            decltype(auto) arg1 = std::get<0>(std::forward_as_tuple(std::forward<Args>(args)...));
                            if constexpr(SelfIsLhs) {
                                return static_cast<ref_type>(*typed) + arg1;
                            }
                            else {
                                return arg1 + static_cast<ref_type>(*typed);
                            }
                        }
                    }
                };
            }

            Ret operator()(Args... args) requires (Qualifiers == fn_qualifiers::none) {
                assert(function != nullptr);
                return function(obj, std::forward<Args>(args)...);
            }

            Ret operator()(Args... args) & requires (Qualifiers == fn_qualifiers::lvalue_ref) {
                assert(function != nullptr);
                return function(obj, std::forward<Args>(args)...);
            }

            Ret operator()(Args... args) && requires (Qualifiers == fn_qualifiers::rvalue_ref) {
                assert(function);
                return function(obj, std::forward<Args>(args)...);
            }

            Ret operator()(Args... args) const requires (Qualifiers == fn_qualifiers::is_const) {
                assert(function);
                return function(obj, std::forward<Args>(args)...);
            }

            Ret operator()(Args... args) const & requires (Qualifiers == (fn_qualifiers::is_const | fn_qualifiers::lvalue_ref)) {
                assert(function);
                return function(obj, std::forward<Args>(args)...);
            }

            Ret operator()(Args... args) const && requires (Qualifiers == (fn_qualifiers::is_const | fn_qualifiers::rvalue_ref)) {
                assert(function);
                return function(obj, std::forward<Args>(args)...);
            }

            explicit operator bool() const noexcept { return function != nullptr; }

            ~vtable_function() = default;
          private:
            constexpr vtable_function() noexcept = default;
            vtable_function(const vtable_function&) noexcept = default;
            vtable_function(vtable_function&&) noexcept = default;
            vtable_function& operator=(const vtable_function&) noexcept = default;
            vtable_function& operator=(vtable_function&&) noexcept = default;

            friend struct vtable;
            friend class duck<Tags...>;
        };
      protected:
        template <std::meta::info Tag>
        consteval static std::meta::info generate_vtable_function() {
            constexpr static std::string_view name = [:template_arguments_of(Tag)[0]:];
            constexpr static auto full_sig = template_arguments_of(Tag)[1];
            constexpr static auto sig = remove_noexcept(remove_fn_qualifiers(template_arguments_of(Tag)[1]));
            constexpr static auto qualifiers = detail::qualifiers_of(full_sig);

            return std::meta::data_member_spec(substitute(^^vtable_function, {sig, ^^qualifiers}), {.name = name});
        }

        template <std::meta::info Tag>
        consteval static std::meta::info generate_vtable_operator() {
            constexpr static auto full_sig = template_arguments_of(Tag)[1];
            constexpr static bool self_is_lhs = remove_cvref(substitute(^^fn_arg_t, {full_sig, std::meta::reflect_constant(0)})) == ^^self;

            constexpr static auto after_remove_self = detail::remove_arg(full_sig, ^^self);
            constexpr static bool is_unary = extract<std::size_t>(substitute(^^fn_arg_count_v, {remove_fn_qualifiers(after_remove_self)})) == 0;

            const auto name = std::string{"__rjk_"} + (is_unary ? "unary_" : (self_is_lhs ? "lhs_" : "rhs_"))
                + enum_to_string([:template_arguments_of(Tag)[0]:]);

            // TODO: Do we need remove_noexcept here?
            constexpr static auto sig = remove_noexcept(remove_fn_qualifiers(
                is_unary ? after_remove_self : detail::substitute_fn_args(after_remove_self, ^^this_duck_t, ^^duck<Tags...>)
            ));
            constexpr static auto qualifiers = detail::qualifiers_of_target(full_sig, ^^self);

            return std::meta::data_member_spec(substitute(^^vtable_function, {sig, ^^qualifiers}), {.name = name});
        }

        consteval {
            std::vector<std::meta::info> members{};
            template for (constexpr auto tag : {^^Tags...}) {
                if constexpr(template_of(tag) == ^^has_fn) {
                    members.push_back(generate_vtable_function<tag>());
                }
                else if constexpr(template_of(tag) == ^^has_op) {
                    members.push_back(generate_vtable_operator<tag>());
                }
            }
            define_aggregate(^^vtable, members);
        }

        constexpr static auto ctx = std::meta::access_context::current();
        constexpr static auto vtable_members = define_static_array(nonstatic_data_members_of(^^detail::duck_base<Tags...>::vtable, ctx));
    };
}

    struct bad_duck_access : std::exception {
        const char* what() const noexcept override {
            return "type mismatch";
        }
    };

    template <rjk::duck_tag... Tags>
    class duck : detail::duck_base<Tags...>, public detail::duck_base<Tags...>::vtable {
      private:
        template <typename T>
        struct init_tag{};

        using detail::duck_base<Tags...>::ctx;
        using detail::duck_base<Tags...>::vtable_members;
      public:
        constexpr duck() = default;

        template <typename T> requires (satisfies_tags<std::decay_t<T>, duck, Tags...>())
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

        duck(const duck& other) {
            copy_from(other);
        }

        duck& operator=(const duck& other) {
            if (this != &other) {
                copy_from(other);
            }
            return *this;
        }

        duck(duck&& other) noexcept
            : m_underlying(std::move(other.m_underlying)) {
            template for (constexpr auto member : vtable_members) {
                this->[:member:] = std::move(other.[:member:]);
            }

            if (m_underlying.using_sbo()) {
                update_vtable_context();
            }
        }

        duck& operator=(duck&& other) noexcept {
            if (this != &other) {
                m_underlying = std::move(other.m_underlying);

                template for (constexpr auto member : vtable_members) {
                    this->[:member:] = std::move(other.[:member:]);
                }

                if (m_underlying.using_sbo()) {
                    update_vtable_context();
                }
            }
            return *this;
        }

        ~duck() = default;
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
            template for (constexpr auto member : vtable_members) {
                this->[:member:] = {};
            }
        }

        void swap(duck& other) noexcept {
            std::swap(m_underlying, other.m_underlying);

            template for (constexpr auto member : vtable_members) {
                auto tmp = std::move(other.[:member:]);
                other.[:member:] = std::move(this->[:member:]);
                this->[:member:] = std::move(tmp);
            }

            update_vtable_context();
            other.update_vtable_context();
        }

        friend void swap(duck& lhs, duck& rhs) noexcept {
            lhs.swap(rhs);
        }
      public:
        bool has_value() const noexcept {
            return m_underlying.has_value();
        }

        template <typename T> requires (satisfies_tags<T, duck, Tags...>())
        T* get_if() noexcept {
            if (!m_underlying.has_type<T>()) {
                return nullptr;
            }
            return static_cast<T*>(m_underlying.get());
        }

        template <typename T> requires (satisfies_tags<T, duck, Tags...>())
        const T* get_if() const noexcept {
            if (!m_underlying.has_type<T>()) {
                return nullptr;
            }
            return static_cast<const T*>(m_underlying.get());
        }

        template <typename T> requires (satisfies_tags<T, duck, Tags...>())
        T& get() & {
            if (!m_underlying.has_type<T>()) {
                throw bad_duck_access{};
            }
            return *static_cast<T*>(m_underlying.get());
        }

        template <typename T> requires (satisfies_tags<T, duck, Tags...>())
        const T& get() const & {
            if (!m_underlying.has_type<T>()) {
                throw bad_duck_access{};
            }
            return *static_cast<const T*>(m_underlying.get());
        }

        template <typename T> requires (satisfies_tags<T, duck, Tags...>())
        T&& get() && {
            if (!m_underlying.has_type<T>()) {
                throw bad_duck_access{};
            }
            return std::move(*static_cast<T*>(m_underlying.get()));
        }

        template <typename T> requires (satisfies_tags<T, duck, Tags...>())
        const T&& get() const && {
            if (!m_underlying.has_type<T>()) {
                throw bad_duck_access{};
            }
            return std::move(*static_cast<const T*>(m_underlying.get()));
        }
      private:
        void copy_from(const duck& other) {
            m_underlying = other.m_underlying;
            template for (constexpr auto member : vtable_members) {
                this->[:member:] = other.[:member:];
            }
            update_vtable_context();
        }

        template <typename T, typename... Args>
        std::decay_t<T>* init_from(Args&&... args) noexcept(std::is_nothrow_constructible_v<std::decay_t<T>, Args&&...> && detail::fits_sbo<std::decay_t<T>>) {
            m_underlying.template emplace<T>(std::forward<Args>(args)...);
            return fill_vtable<T>();
        }

        template <typename T, typename... Args>
        duck(init_tag<T>, Args&&... args) noexcept(std::is_nothrow_constructible_v<std::decay_t<T>, Args&&...> && detail::fits_sbo<std::decay_t<T>>)
            : m_underlying(std::in_place_type<T>, std::forward<Args>(args)...) {
            fill_vtable<T>();
        }

        template <typename T, std::meta::info Member>
        void fill_vtable_fn(void* ptr) noexcept {
            // Not used in all if-constexpr paths
            [[maybe_unused]] constexpr static auto stripped_sig = template_arguments_of(dealias(type_of(Member)))[0];

            constexpr static auto full_sig = [] consteval {
                template for (constexpr auto tag : {^^Tags...}) {
                    if constexpr(template_of(tag) == ^^has_fn) {
                        if constexpr(std::string_view{[:template_arguments_of(tag)[0]:]} == identifier_of(Member)) {
                            return remove_noexcept(template_arguments_of(tag)[1]);
                        }
                    }
                }
                return stripped_sig;
            }();

            if constexpr(is_class_type(^^T)) {
                constexpr static auto other_members = define_static_array(members_of(^^std::decay_t<T>, ctx));
                template for (constexpr auto other_member : other_members) {
                    if constexpr(has_identifier(other_member) && is_function(other_member)) {
                        if constexpr(
                            identifier_of(other_member) == identifier_of(Member) &&
                            dealias(remove_noexcept(type_of(other_member))) == full_sig
                        ) {
                            constexpr static auto qualifiers = detail::qualifiers_of(full_sig);
                            using vtable_function_type = [:substitute(^^detail::duck_base<Tags...>::vtable_function, {stripped_sig, ^^qualifiers}):];

                            this->[:Member:] = vtable_function_type::template make_fn<T, other_member>(ptr);
                            break;
                        }
                    }
                }
            }
        }

        template <typename T, std::meta::info Member>
        void fill_vtable_op(void* ptr) noexcept {
            constexpr static auto name = identifier_of(Member);

            template for (constexpr auto tag : {^^Tags...}) {
                if constexpr(template_of(tag) == ^^has_op) {
                    constexpr static auto tag_op   = [:template_arguments_of(tag)[0]:];
                    constexpr static auto tag_full_sig = template_arguments_of(tag)[1];
                    constexpr static bool self_is_lhs  = remove_cvref(substitute(^^fn_arg_t, {tag_full_sig, std::meta::reflect_constant(0)})) == ^^self;

                    constexpr static auto after_remove_self = detail::remove_arg(tag_full_sig, ^^self);
                    constexpr static bool is_unary = extract<std::size_t>(substitute(^^fn_arg_count_v, {remove_fn_qualifiers(after_remove_self)})) == 0;

                    constexpr auto tag_name = define_static_string(std::string{"__rjk_"} + (is_unary ? "unary_" : (self_is_lhs ? "lhs_" : "rhs_")) + enum_to_string(tag_op));
                    if constexpr(tag_name == name) {
                        constexpr static auto qualifiers   = detail::qualifiers_of_target(tag_full_sig, ^^self);

                        constexpr static auto stripped_sig = template_arguments_of(dealias(type_of(Member)))[0];
                        using vtable_function_type = [:substitute(^^detail::duck_base<Tags...>::vtable_function, {stripped_sig, ^^qualifiers}):];

                        [:Member:] = vtable_function_type::template make_op<T, tag_op, self_is_lhs>(ptr);
                    }
                }
            }
        }

        template <typename T>
        std::decay_t<T>* fill_vtable() noexcept {
            auto* ptr = m_underlying.get();

            template for (constexpr auto member : vtable_members) {
                fill_vtable_fn<T, member>(ptr);
                fill_vtable_op<T, member>(ptr);
            }

            return static_cast<std::decay_t<T>*>(ptr);
        }

        void update_vtable_context() noexcept {
            auto* ptr = m_underlying.get();
            template for (constexpr auto member : vtable_members) {
                this->[:member:].obj = ptr;
            }
        }
      private:
        template <std::meta::operators Op, typename Lhs, typename Rhs>
        consteval static bool satisfies_operator(op_overload_kind kind) noexcept {
            if(!has_operator_tag<Op, Tags...>(kind)) {
                return false;
            }

            switch (kind) {
                using enum op_overload_kind;
            case any_kind:
                throw std::logic_error("Must use specific kind");
            case binary_lhs:
                return std::same_as<std::decay_t<Lhs>, duck> && !std::same_as<std::decay_t<Lhs>, std::decay_t<Rhs>>;
            case binary_rhs:
                return std::same_as<std::decay_t<Rhs>, duck> && !std::same_as<std::decay_t<Lhs>, std::decay_t<Rhs>>;
            case unary:
                return std::same_as<std::decay_t<Lhs>, duck>;
            }
            return false;
        }

        template <typename This> requires (satisfies_operator<op_plus, This, void>(op_overload_kind::unary))
        friend decltype(auto) operator+(This&& operand) {
            return std::forward<This>(operand).__rjk_unary_op_plus();
        }

        template <typename This, typename R> requires (satisfies_operator<op_plus, This, R>(op_overload_kind::binary_lhs))
        friend decltype(auto) operator+(This&& lhs, R&& rhs) {
            return std::forward<This>(lhs).__rjk_lhs_op_plus(std::forward<R>(rhs));
        }

        template <typename L, typename This> requires (satisfies_operator<op_plus, L, This>(op_overload_kind::binary_rhs))
        friend decltype(auto) operator+(L&& lhs, This&& rhs) {
            return std::forward<This>(rhs).__rjk_rhs_op_plus(std::forward<L>(lhs));
        }
      private:
        detail::storage m_underlying{};
    };
}

#endif