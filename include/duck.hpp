// (No support for formatting reflection currently)
// clang-format off

#ifndef RJK_DUCK_HPP
#define RJK_DUCK_HPP

#include "duck.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <concepts>
#include <functional>
#include <meta>
#include <print>
#include <ranges>
#include <string_view>

#include "duck_tags.hpp"
#include "detail/fn_traits.hpp"
#include "detail/substitute_fn_traits.hpp"
#include "detail/storage.hpp"
#include "detail/vtable_fn_maker.hpp"

namespace rjk {
    template <duck_tag... Tags>
    class duck;
namespace detail {
    template <duck_tag... Tags>
    class duck_base {
      protected:
        constexpr static auto ctx = std::meta::access_context::current();

        struct static_duck_vtable;

        consteval static std::string index_to_slot_name(std::size_t index) {
            std::string result = "slot_";
            if (index == 0UZ) return result + '0';
            std::string digits;
            while (index > 0UZ) {
                digits += ('0' + index % 10UZ);
                index /= 10UZ;
            }
            std::ranges::reverse(digits);
            return result + digits;
        }

        consteval {
            std::vector<std::meta::info> members{};

            auto index = 0UZ;
            template for (constexpr auto tag : {^^Tags...}) {
                constexpr static auto full_sig = template_arguments_of(tag)[1];

                if constexpr (template_of(tag) == ^^has_fn) {
                    constexpr static auto erased_ptr_type =
                    static_cast<bool>((qualifiers_of(full_sig) & fn_qualifiers::is_const))
                    ?   ^^const void* : ^^void*;

                    constexpr static auto sig = remove_noexcept(
                        remove_fn_qualifiers(full_sig));
                    constexpr static auto ptr_type = substitute(^^fn_to_ptr_t,
                        {substitute(^^detail::prepend_arg_t, {erased_ptr_type, sig})});
                    members.push_back(data_member_spec(ptr_type, {
                        .name = index_to_slot_name(index)
                    }));
                }
                else if constexpr (template_of(tag) == ^^has_op) {
                    constexpr static auto erased_ptr_type =
                    static_cast<bool>((qualifiers_of_target(full_sig, ^^self) & fn_qualifiers::is_const))
                    ? ^^const void* : ^^void*;

                    constexpr static auto after_remove_self = detail::remove_arg(full_sig, ^^self);
                    constexpr static auto sig = remove_noexcept(remove_fn_qualifiers(
                        detail::substitute_fn_args(after_remove_self, ^^self, ^^duck<Tags...>)
                    ));
                    constexpr static auto ptr_type = substitute(^^fn_to_ptr_t,
                        {substitute(^^detail::prepend_arg_t, {erased_ptr_type, sig})});
                    members.push_back(data_member_spec(ptr_type, {
                        .name = index_to_slot_name(index)
                    }));
                }
                index++;
            }

            define_aggregate(^^static_duck_vtable, members);
        }

        template <typename T>
        constexpr static auto static_vtable_for = [] {
            static_duck_vtable table{};

            constexpr static auto slots = define_static_array(
                nonstatic_data_members_of(^^duck_base<Tags...>::static_duck_vtable, ctx)
            );

            constexpr static std::array tags{^^Tags...};

            template for (constexpr auto index : std::views::indices(sizeof...(Tags))) {
                constexpr static auto tag = tags[index];

                if constexpr (template_of(tag) == ^^has_fn) {
                    constexpr static auto member_name = template_arguments_of(tag)[0];
                    constexpr static auto full_sig    = template_arguments_of(tag)[1];
                    constexpr static auto qualifiers  = qualifiers_of(full_sig);

                    constexpr static auto T_member = [] consteval -> std::meta::info {
                        template for (constexpr auto m : define_static_array(members_of(^^T, ctx))) {
                            if constexpr (has_identifier(m) && is_function(m)) {
                                if constexpr (identifier_of(m) == [:member_name:]) {
                                    if constexpr (dealias(remove_noexcept(type_of(m))) == remove_noexcept(full_sig)) {
                                        return m;
                                    }
                                }
                            }
                        }
                        throw std::logic_error("Could not find member");
                    }();

                    constexpr static auto sig = remove_noexcept(
                        remove_fn_qualifiers(full_sig));
                    constexpr static auto fn_maker = substitute(^^vtable_fn_maker,
                        {sig, ^^qualifiers, ^^T_member, ^^T});

                    table.[:slots[index]:] = [:fn_maker:]::make();
                }
                else if constexpr (template_of(tag) == ^^has_op) {
                    constexpr static auto full_sig    = template_arguments_of(tag)[1];
                    constexpr static auto qualifiers  = qualifiers_of_target(full_sig, ^^self);
                    constexpr static auto tag_op      = template_arguments_of(tag)[0];
                    constexpr static bool self_is_lhs = remove_cvref(substitute(^^fn_arg_t, {full_sig, std::meta::reflect_constant(0UZ)})) == ^^self;

                    constexpr static auto after_remove_self = remove_arg(full_sig, ^^self);
                    constexpr static bool is_unary = extract<std::size_t>(substitute(^^fn_arg_count_v, {remove_fn_qualifiers(after_remove_self)})) == 0;

                    constexpr static auto sig = remove_noexcept(remove_fn_qualifiers(
                        is_unary ? after_remove_self
                                 : substitute_fn_args(after_remove_self, ^^self, ^^duck<Tags...>)
                    ));

                    constexpr static auto op_maker = substitute(^^vtable_op_maker,
                        {sig, ^^qualifiers, tag_op, ^^self_is_lhs, ^^T});

                    table.[:slots[index]:] = [:op_maker:]::make();
                }
            }

            return table;
        }();
      protected:
        template <typename VtableTag>
        struct vtable_function_wrapper;

        template <std::size_t VtableIndex, fn_qualifiers Qualifiers, typename Func>
        class vtable_function;

        template <std::size_t VtableIndex, fn_qualifiers Qualifiers, typename Ret, typename... Args>
        class vtable_function<VtableIndex, Qualifiers, Ret(Args...)> {
          public:
            constexpr static auto static_vtable_member = [] {
                const auto range = std::meta::nonstatic_data_members_of(^^static_duck_vtable, ctx);
                auto it = std::ranges::find_if(range,
                    [](auto info) {
                        return identifier_of(info) == index_to_slot_name(VtableIndex);
                    });
                if (it == range.end()) {
                    throw std::logic_error("Could not find index in static_duck_vtable");
                }
                return *it;
            }();

            Ret operator()(Args... args) requires (Qualifiers == fn_qualifiers::none);

            Ret operator()(Args... args) & requires (Qualifiers == fn_qualifiers::lvalue_ref);

            Ret operator()(Args... args) && requires (Qualifiers == fn_qualifiers::rvalue_ref);

            Ret operator()(Args... args) const requires (Qualifiers == fn_qualifiers::is_const);

            Ret operator()(Args... args) const & requires (Qualifiers == (fn_qualifiers::is_const | fn_qualifiers::lvalue_ref));

            Ret operator()(Args... args) const && requires (Qualifiers == (fn_qualifiers::is_const | fn_qualifiers::rvalue_ref));

            ~vtable_function() = default;
          private:
            constexpr vtable_function() noexcept = default;
            vtable_function(const vtable_function&) noexcept = default;
            vtable_function(vtable_function&&) noexcept = default;
            vtable_function& operator=(const vtable_function&) noexcept = default;
            vtable_function& operator=(vtable_function&&) noexcept = default;

            duck<Tags...>* trace_to_duck();
            const duck<Tags...>* trace_to_duck() const;

            template <typename VtableTag>
            friend struct vtable_function_wrapper;
            friend class duck<Tags...>;
        };
      protected:
        template <std::meta::info Tag, std::size_t Index>
        consteval static std::meta::info generate_vtable_function() {
            constexpr static std::string_view name = [:template_arguments_of(Tag)[0]:];
            constexpr static auto full_sig = template_arguments_of(Tag)[1];
            constexpr static auto sig = remove_noexcept(remove_fn_qualifiers(template_arguments_of(Tag)[1]));
            constexpr static auto qualifiers = detail::qualifiers_of(full_sig);

            return std::meta::data_member_spec(
                substitute(^^vtable_function, {std::meta::reflect_constant(Index), ^^qualifiers, sig}),
            {.name = name, .no_unique_address = true});
        }

        template <std::meta::info Tag, std::size_t Index>
        consteval static std::meta::info generate_vtable_operator() {
            constexpr static auto full_sig = template_arguments_of(Tag)[1];
            constexpr static bool self_is_lhs = remove_cvref(substitute(^^fn_arg_t, {full_sig, std::meta::reflect_constant(0)})) == ^^self;

            constexpr static auto after_remove_self = detail::remove_arg(full_sig, ^^self);
            constexpr static bool is_unary = extract<std::size_t>(substitute(^^fn_arg_count_v, {remove_fn_qualifiers(after_remove_self)})) == 0;

            const auto name = std::string{"__rjk_"} + (is_unary ? "unary_" : (self_is_lhs ? "lhs_" : "rhs_"))
                + enum_to_string([:template_arguments_of(Tag)[0]:]);

            // TODO: Do we need remove_noexcept here?
            constexpr static auto sig = remove_noexcept(remove_fn_qualifiers(
                is_unary ? after_remove_self : detail::substitute_fn_args(after_remove_self, ^^self, ^^duck<Tags...>)
            ));
            constexpr static auto qualifiers = detail::qualifiers_of_target(full_sig, ^^self);

            return std::meta::data_member_spec(
                substitute(^^vtable_function, {std::meta::reflect_constant(Index), ^^qualifiers, sig}),
            {.name = name, .no_unique_address = true});
        }

        consteval {
            constexpr static std::array tags{^^Tags...};
            template for (constexpr auto index : std::views::indices(sizeof...(Tags))) {
                constexpr static auto tag = tags[index];
                if constexpr(template_of(tag) == ^^has_fn) {
                    define_aggregate(
                        substitute(^^vtable_function_wrapper, {tag}),
                        {generate_vtable_function<tag, index>()});
                }
                else if constexpr(template_of(tag) == ^^has_op) {
                    define_aggregate(
                        substitute(^^vtable_function_wrapper, {tag}),
                        {generate_vtable_operator<tag, index>()});
                }
            }
        }

        template <typename... VtableFuncs>
        struct vtable_wrapper_impl : VtableFuncs... {};
        using vtable_wrapper = vtable_wrapper_impl<vtable_function_wrapper<Tags>...>;

        static_assert((std::is_standard_layout_v<vtable_function_wrapper<Tags>> && ...));

        constexpr static auto vtable_members = define_static_array(
            std::meta::bases_of(^^vtable_wrapper, ctx)
            | std::views::transform([](auto base) {
                return std::meta::nonstatic_data_members_of(type_of(base), ctx)[0];
            })
        );
    };
}

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

        using detail::duck_base<Tags...>::ctx;
        using detail::duck_base<Tags...>::vtable_members;
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
            template for (constexpr auto member : vtable_members) {
                this->[:member:] = {};
            }
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
        template <typename T, typename... Args>
        std::decay_t<T>* init_from(Args&&... args) noexcept(std::is_nothrow_constructible_v<std::decay_t<T>, Args&&...> && detail::fits_sbo<std::decay_t<T>>) {
            m_underlying.emplace<T>(std::forward<Args>(args)...);
            m_vtable = &detail::duck_base<Tags...>::template static_vtable_for<T>;

            return static_cast<std::decay_t<T>*>(m_underlying.get());
        }

        template <typename T, typename... Args>
        duck(init_tag<T>, Args&&... args) noexcept(std::is_nothrow_constructible_v<std::decay_t<T>, Args&&...> && detail::fits_sbo<std::decay_t<T>>)
            : m_underlying(std::in_place_type<T>, std::forward<Args>(args)...)
            , m_vtable(&detail::duck_base<Tags...>::template static_vtable_for<T>) {
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
        const detail::duck_base<Tags...>::static_duck_vtable* m_vtable = nullptr;

    };

namespace detail {

    template <duck_tag... Tags>
    template <std::size_t VtableIndex, fn_qualifiers Qualifiers, typename Ret, typename... Args>
    duck<Tags...>* duck_base<Tags...>::vtable_function<VtableIndex, Qualifiers, Ret(Args...)>
    ::trace_to_duck() {
        auto* wrapper = reinterpret_cast<vtable_function_wrapper<Tags...[VtableIndex]>*>(this);
        return static_cast<duck<Tags...>*>(wrapper);
    }

    template <duck_tag... Tags>
    template <std::size_t VtableIndex, fn_qualifiers Qualifiers, typename Ret, typename... Args>
    const duck<Tags...>* duck_base<Tags...>::vtable_function<VtableIndex, Qualifiers, Ret(Args...)>
    ::trace_to_duck() const {
        const auto* wrapper = reinterpret_cast<const vtable_function_wrapper<Tags...[VtableIndex]>*>(this);
        return static_cast<const duck<Tags...>*>(wrapper);
    }

    template <duck_tag... Tags>
    template <std::size_t VtableIndex, fn_qualifiers Qualifiers, typename Ret, typename... Args>
    Ret duck_base<Tags...>::vtable_function<VtableIndex, Qualifiers, Ret(Args...)>
    ::operator()(Args... args) requires (Qualifiers == fn_qualifiers::none) {
        auto* obj = trace_to_duck();
        return obj->m_vtable->[:static_vtable_member:](
            obj->m_underlying.get(),
            std::forward<Args>(args)...
        );
    }

    template <duck_tag... Tags>
    template <std::size_t VtableIndex, fn_qualifiers Qualifiers, typename Ret, typename... Args>
    Ret duck_base<Tags...>::vtable_function<VtableIndex, Qualifiers, Ret(Args...)>
    ::operator()(Args... args) & requires (Qualifiers == fn_qualifiers::lvalue_ref) {
        auto* obj = trace_to_duck();
        return obj->m_vtable->[:static_vtable_member:](
            obj->m_underlying.get(),
            std::forward<Args>(args)...
        );
    }

    template <duck_tag... Tags>
    template <std::size_t VtableIndex, fn_qualifiers Qualifiers, typename Ret, typename... Args>
    Ret duck_base<Tags...>::vtable_function<VtableIndex, Qualifiers, Ret(Args...)>
    ::operator()(Args... args) && requires (Qualifiers == fn_qualifiers::rvalue_ref) {
        auto* obj = trace_to_duck();
        return obj->m_vtable->[:static_vtable_member:](
            obj->m_underlying.get(),
            std::forward<Args>(args)...
        );
    }

    template <duck_tag... Tags>
    template <std::size_t VtableIndex, fn_qualifiers Qualifiers, typename Ret, typename... Args>
    Ret duck_base<Tags...>::vtable_function<VtableIndex, Qualifiers, Ret(Args...)>
    ::operator()(Args... args) const requires (Qualifiers == fn_qualifiers::is_const) {
        const auto* obj = trace_to_duck();
        return obj->m_vtable->[:static_vtable_member:](
            obj->m_underlying.get(),
            std::forward<Args>(args)...
        );
    }

    template <duck_tag... Tags>
    template <std::size_t VtableIndex, fn_qualifiers Qualifiers, typename Ret, typename... Args>
    Ret duck_base<Tags...>::vtable_function<VtableIndex, Qualifiers, Ret(Args...)>
    ::operator()(Args... args) const &
    requires (Qualifiers == (fn_qualifiers::is_const | fn_qualifiers::lvalue_ref)) {
        const auto* obj = trace_to_duck();
        return obj->m_vtable->[:static_vtable_member:](
            obj->m_underlying.get(),
            std::forward<Args>(args)...
        );
    }

    template <duck_tag... Tags>
    template <std::size_t VtableIndex, fn_qualifiers Qualifiers, typename Ret, typename... Args>
    Ret duck_base<Tags...>::vtable_function<VtableIndex, Qualifiers, Ret(Args...)>
    ::operator()(Args... args) const && requires (Qualifiers == (fn_qualifiers::is_const | fn_qualifiers::rvalue_ref)) {
        const auto* obj = trace_to_duck();
        return obj->m_vtable->[:static_vtable_member:](
            obj->m_underlying.get(),
            std::forward<Args>(args)...
        );
    }
}
}

#endif