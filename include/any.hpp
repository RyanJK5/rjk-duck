#pragma once

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

#include "any_tags.hpp"
#include "detail/substitute_fn_traits.hpp"

namespace rjk {
    namespace detail {
        class storage {
        public:
            constexpr static std::size_t sbo_size = 32;
            
            template <typename T>
            consteval static bool fits_sbo() {
                return (sizeof(T) <= sbo_size) && 
                    (alignof(T) <= alignof(std::max_align_t)) && 
                    std::is_nothrow_move_constructible_v<T>;
            }

            constexpr storage() = default;

            template <typename T, typename... Args>
            explicit storage(std::in_place_type_t<T>, Args&&... args)
                noexcept(std::is_nothrow_constructible_v<std::decay_t<T>, Args&&...> && fits_sbo<std::decay_t<T>>()) {
                init<std::decay_t<T>>(std::forward<Args>(args)...);
            }

            template <typename T, typename... Args>
            void emplace(Args&&... args)
                noexcept(std::is_nothrow_constructible_v<std::decay_t<T>, Args&&...> && fits_sbo<std::decay_t<T>>()) {
                reset();
                init<std::decay_t<T>>(std::forward<Args>(args)...);
            }

            storage(const storage& other) {
                copy_from(other);
            }

            storage(storage&& other) noexcept
                : destroy(std::exchange(other.destroy, nullptr))
                , move(std::exchange(other.move, nullptr))
                , copy(std::exchange(other.copy, nullptr))
                , is_inline(other.is_inline) {
                if (move) {
                    move(other, *this);
                }
            }

            storage& operator=(const storage& other) {
                if (this != &other) {
                    if (destroy) {
                        destroy(*this);
                    }
                    copy_from(other);
                }
                return *this;
            }

            storage& operator=(storage&& other) noexcept {
                if (this != & other) {
                    if (destroy) {
                        destroy(*this);
                    }

                    is_inline = other.is_inline;
                    destroy = std::exchange(other.destroy, nullptr);
                    move = std::exchange(other.move, nullptr);
                    copy = std::exchange(other.copy, nullptr);

                    if (move) {
                        move(other, *this);
                    }
                }
                return *this;
            }

            ~storage() {
                if (destroy) {
                    destroy(*this);
                }
            }

            constexpr bool using_sbo() const noexcept { return is_inline; }

            void* get() noexcept {
                return is_inline ? std::launder(buf.data()) : ptr;
            }

            const void* get() const noexcept {
                return is_inline ? std::launder(buf.data()) : ptr;
            }

            bool has_value() const noexcept {
                return destroy != nullptr;
            }

            template <typename T>
            constexpr bool has_type() const noexcept {
                return destroy == call_destroy<T>;
            }
            
            void reset() noexcept {
                if (destroy) {
                    destroy(*this);
                }
                destroy = nullptr;
                move = nullptr;
                copy = nullptr;
                is_inline = false;
            }
        private:
            template <typename T>
            static void call_destroy(storage& obj) noexcept {
                if constexpr (fits_sbo<T>()) {
                    std::destroy_at(std::launder(reinterpret_cast<T*>(obj.buf.data())));
                } else {
                    delete static_cast<T*>(obj.ptr);
                }
            }

            template <typename T, typename... Args>
            void init(Args&&... args) {
                constexpr static bool use_sbo = fits_sbo<T>();
                is_inline = use_sbo;

                if constexpr(use_sbo) {
                    std::construct_at(reinterpret_cast<T*>(buf.data()), std::forward<Args>(args)...);
                }
                else {
                    ptr = new T(std::forward<Args>(args)...);
                }

                destroy = &call_destroy<T>;

                move = [](storage& src, storage& dest) noexcept {
                    if constexpr(use_sbo) {
                        std::construct_at(reinterpret_cast<T*>(dest.buf.data()), 
                            std::move(*std::launder(reinterpret_cast<T*>(src.buf.data()))));
                        std::destroy_at(std::launder(reinterpret_cast<T*>(src.buf.data())));
                    }
                    else {
                        dest.ptr = std::exchange(src.ptr, nullptr);
                    }
                };

                if constexpr (std::is_copy_constructible_v<T>) {
                    copy = [](const storage& src, storage& dest) {
                        if constexpr(use_sbo) {
                            std::construct_at(reinterpret_cast<T*>(dest.buf.data()), *std::launder(reinterpret_cast<const T*>(src.buf.data())));
                        }
                        else {
                            dest.ptr = new T(*static_cast<T*>(src.ptr));
                        }
                    };
                }
            }

            void copy_from(const storage& other) {
                is_inline = other.is_inline;
                destroy = other.destroy;
                move = other.move;
                copy = other.copy;

                if (other.copy) {
                    copy(other, *this);
                }
            }
        private:
            void (*destroy)(storage&) noexcept = nullptr;
            void (*move)(storage&, storage&) noexcept = nullptr;
            void (*copy)(const storage&, storage&) = nullptr;

            union {
                alignas(std::max_align_t) std::array<std::byte, sbo_size> buf;
                void* ptr;
            };

            bool is_inline = false;
        };

        struct vtable_anchor {
            constexpr vtable_anchor() = default;
            ~vtable_anchor() = default;
            vtable_anchor(const vtable_anchor&) = delete;
            vtable_anchor(vtable_anchor&&) = delete;
            void operator=(const vtable_anchor&) = delete;
            void operator=(vtable_anchor&&) = delete;
        };
    }

    struct bad_any_access : std::exception {
        const char* what() const noexcept override {
            return "type mismatch";
        }
    };

    template <rjk::any_tag... Tags>
    class any {
      private:
        template <typename Func, detail::fn_qualifiers Qualifiers>
        class vtable_function;

        template <detail::fn_qualifiers Qualifiers, typename Ret, typename... Args>
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
                            static_cast<bool>(Qualifiers & detail::fn_qualifiers::is_const), 
                            const T, 
                            T
                        >;
                        using ref_type = std::conditional_t<
                            static_cast<bool>(Qualifiers & detail::fn_qualifiers::rvalue_ref),
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

            template <typename T, std::meta::operators Op, bool SelfIsLhs, bool HasOther>
            static vtable_function make_op(void* underlying) noexcept {
                return {
                    underlying,
                    [](void* context, Args... args) -> Ret {
                        using obj_type = std::conditional_t<
                            static_cast<bool>(Qualifiers & detail::fn_qualifiers::is_const), 
                            const T, 
                            T
                        >;
                        using ref_type = std::conditional_t<
                            static_cast<bool>(Qualifiers & detail::fn_qualifiers::rvalue_ref),
                            obj_type&&,
                            obj_type&
                        >;

                        auto* typed = static_cast<obj_type*>(context);

                        if constexpr(sizeof...(Args) == 0UZ) {
                            if constexpr(Op == rjk::op_plus) return +static_cast<ref_type>(*typed);
                        }
                        if constexpr(Op == rjk::op_plus) {
                            decltype(auto) arg1 = resolve_operator_arg<ref_type, HasOther>(std::get<0>(std::forward_as_tuple(std::forward<Args>(args)...)));
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

            Ret operator()(Args... args) requires (Qualifiers == detail::fn_qualifiers::none) {
                assert(function);
                return function(obj, std::forward<Args>(args)...);
            }

            Ret operator()(Args... args) & requires (Qualifiers == detail::fn_qualifiers::lvalue_ref) {
                assert(function);
                return function(obj, std::forward<Args>(args)...);
            }

            Ret operator()(Args... args) && requires (Qualifiers == detail::fn_qualifiers::rvalue_ref) {
                assert(function);
                return function(obj, std::forward<Args>(args)...);
            }

            Ret operator()(Args... args) const requires (Qualifiers == detail::fn_qualifiers::is_const) {
                assert(function);
                return function(obj, std::forward<Args>(args)...);
            }

            Ret operator()(Args... args) const & requires (Qualifiers == (detail::fn_qualifiers::is_const | detail::fn_qualifiers::lvalue_ref)) {
                assert(function);
                return function(obj, std::forward<Args>(args)...);
            }

            Ret operator()(Args... args) const && requires (Qualifiers == (detail::fn_qualifiers::is_const | detail::fn_qualifiers::rvalue_ref)) {
                assert(function);
                return function(obj, std::forward<Args>(args)...);
            }

            explicit operator bool() const noexcept { return function != nullptr; }
            
            ~vtable_function() = default;
          private:
            constexpr vtable_function() = default;
            vtable_function(const vtable_function&) noexcept = default;
            vtable_function(vtable_function&&) noexcept = default;
            vtable_function& operator=(const vtable_function&) noexcept = default;
            vtable_function& operator=(vtable_function&&) noexcept = default;

            friend class any;
        };
      private:
        struct vtable;
        constexpr static auto ctx = std::meta::access_context::current();
        constexpr static auto copy_blocker_identifier = std::string_view{"__rjk_copy_blocker"};

        template <std::meta::info Tag>
        consteval static std::meta::info generate_vtable_function() {
            constexpr static auto name = [:template_arguments_of(Tag)[0]:];
            constexpr static auto full_sig = template_arguments_of(Tag)[1];
            constexpr static auto sig = remove_noexcept(remove_fn_qualifiers(template_arguments_of(Tag)[1]));
            constexpr static auto qualifiers = detail::qualifiers_of(full_sig);

            return std::meta::data_member_spec(substitute(^^vtable_function, {sig, ^^qualifiers}), {.name = name});
        }

        template <std::meta::info Tag>
        consteval static std::meta::info generate_vtable_operator() {
            constexpr static auto full_sig = template_arguments_of(Tag)[1];
            constexpr static bool self_is_lhs = remove_cvref(substitute(^^fn_arg_t, {full_sig, std::meta::reflect_constant(0)})) == ^^self;
            constexpr auto name = std::string{"__rjk_"} + enum_to_string([:template_arguments_of(Tag)[0]:]) + (self_is_lhs ? "1" : "2");

            constexpr static auto after_remove_self = detail::remove_arg(full_sig, ^^self);
            constexpr static bool is_unary = extract<std::size_t>(substitute(^^fn_arg_count_v, {remove_fn_qualifiers(after_remove_self)})) == 0;
            constexpr static bool has_other = !is_unary &&
                remove_cvref(substitute(^^fn_arg_t, {full_sig, std::meta::reflect_constant(self_is_lhs ? 1 : 0)})) == ^^other;

            // TODO: Do we need remove_noexcept here?
            constexpr static auto sig = remove_noexcept(remove_fn_qualifiers(
                detail::substitute_fn_args(
                    is_unary ? after_remove_self : detail::substitute_fn_args(after_remove_self, ^^other, ^^void*, false),
                    ^^other, ^^void*, false
                )
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

            members.push_back(std::meta::data_member_spec(^^detail::vtable_anchor, {
                .name = copy_blocker_identifier,
                .no_unique_address = true
            }));
            define_aggregate(^^vtable, members);
        }

        constexpr static auto vtable_members = define_static_array(nonstatic_data_members_of(^^vtable, ctx));
        
        template <typename T>
        struct init_tag{};
      public:
        constexpr any() noexcept = default;

        template <typename T> requires satisfies_tags<std::decay_t<T>, Tags...>
        any(T&& obj) 
            noexcept(std::is_nothrow_constructible_v<std::decay_t<T>, T&&> && detail::storage::fits_sbo<std::decay_t<T>>())
            : any(init_tag<std::decay_t<T>>{}, std::forward<T>(obj)) { }

        template <typename T, typename... Args> requires satisfies_tags<std::decay_t<T>, Tags...>
        explicit any(std::in_place_type_t<T>, Args&&... args) 
            noexcept(std::is_nothrow_constructible_v<std::decay_t<T>, Args&&...> && detail::storage::fits_sbo<std::decay_t<T>>())
            : any(init_tag<std::decay_t<T>>{},std::forward<Args>(args)...) { }

        template <typename T, typename U, typename... Args> requires satisfies_tags<std::decay_t<T>, Tags...>
        explicit any(std::in_place_type_t<T>, std::initializer_list<U> il, Args&&... args) 
            noexcept(std::is_nothrow_constructible_v<std::decay_t<T>, std::initializer_list<U>, Args&&...> && detail::storage::fits_sbo<std::decay_t<T>>()) 
            : any(init_tag<std::decay_t<T>>{}, il, std::forward<Args>(args)...) { }

        template <typename T> requires satisfies_tags<std::decay_t<T>, Tags...>
        any& operator=(T&& obj) 
            noexcept(std::is_nothrow_constructible_v<std::decay_t<T>, T&&> && detail::storage::fits_sbo<std::decay_t<T>>()) {
            init_from<std::decay_t<T>>(std::forward<T>(obj));
            return *this;
        }

        any(const any& other) {
            copy_from(other);
        }

        any& operator=(const any& other) {
            if (this != &other) {
                copy_from(other);   
            }
            return *this;
        }

        any(any&& other) noexcept
            : m_underlying(std::move(other.m_underlying)) {
            template for (constexpr auto member : vtable_members) {
                if constexpr(identifier_of(member) != copy_blocker_identifier) {
                    m_vtable.[:member:] = std::move(other.m_vtable.[:member:]);
                }
            }

            if (m_underlying.using_sbo()) {
                update_vtable_context();
            }
        }

        any& operator=(any&& other) noexcept {
            if (this != &other) {
                m_underlying = std::move(other.m_underlying);
                
                template for (constexpr auto member : vtable_members) {
                    if constexpr(identifier_of(member) != copy_blocker_identifier) {
                        m_vtable.[:member:] = std::move(other.m_vtable.[:member:]);
                    }
                }
                
                if (m_underlying.using_sbo()) {
                    update_vtable_context();
                }
            }
            return *this;
        }

        ~any() = default;
      public:
        template <typename T, typename... Args> requires satisfies_tags<T, Tags...>
        std::decay_t<T>& emplace(Args&&... args) 
            noexcept(std::is_nothrow_constructible_v<std::decay_t<T>, Args&&...> && detail::storage::fits_sbo<std::decay_t<T>>()) {
            return *init_from<std::decay_t<T>>(std::forward<Args>(args)...);
        }

        template <typename T, typename U, typename... Args> requires satisfies_tags<T, Tags...>
        std::decay_t<T>& emplace(std::initializer_list<U> il, Args&&... args) 
            noexcept(std::is_nothrow_constructible_v<std::decay_t<T>, std::initializer_list<U>, Args&&...> && detail::storage::fits_sbo<std::decay_t<T>>()) {
            return *init_from<std::decay_t<T>>(il, std::forward<Args>(args)...);
        }

        void reset() noexcept {
            m_underlying.reset();
            template for (constexpr auto member : vtable_members) {
                if constexpr(identifier_of(member) != copy_blocker_identifier) {
                    m_vtable.[:member:] = {};
                }
            }
        }

        void swap(any& other) noexcept {
            std::swap(m_underlying, other.m_underlying);
            
            template for (constexpr auto member : vtable_members) {
                    if constexpr(identifier_of(member) != copy_blocker_identifier) {
                        auto tmp = std::move(other.m_vtable.[:member:]);
                        other.m_vtable.[:member:] = std::move(m_vtable.[:member:]);
                        m_vtable.[:member:] = std::move(tmp);
                    }
            }
            
            update_vtable_context();
            other.update_vtable_context();
        }

        friend void swap(any& lhs, any& rhs) noexcept {
            lhs.swap(rhs);
        }
      public:
        bool has_value() const noexcept {
            return m_underlying.has_value();
        }

        template <typename T> requires satisfies_tags<T, Tags...>
        T* get_if() noexcept {
            if (!m_underlying.has_type<T>()) {
                return nullptr;
            }
            return static_cast<T*>(m_underlying.get());
        }

        template <typename T> requires satisfies_tags<T, Tags...>
        const T* get_if() const noexcept {
            if (!m_underlying.has_type<T>()) {
                return nullptr;
            }
            return static_cast<const T*>(m_underlying.get());
        }

        template <typename T> requires satisfies_tags<T, Tags...>
        T& get() & {
            if (!m_underlying.has_type<T>()) {
                throw bad_any_access{};
            }
            return *static_cast<T*>(m_underlying.get());
        }

        template <typename T> requires satisfies_tags<T, Tags...>
        const T& get() const & {
            if (!m_underlying.has_type<T>()) {
                throw bad_any_access{};
            }
            return *static_cast<const T*>(m_underlying.get());
        }

        template <typename T> requires satisfies_tags<T, Tags...>
        T&& get() && {
            if (!m_underlying.has_type<T>()) {
                throw bad_any_access{};
            }
            return std::move(*static_cast<T*>(m_underlying.get()));
        }

        template <typename T> requires satisfies_tags<T, Tags...>
        const T&& get() const && {
            if (!m_underlying.has_type<T>()) {
                throw bad_any_access{};
            }
            return std::move(*static_cast<const T*>(m_underlying.get()));
        }
      public:
        auto* operator->() noexcept { 
            assert(has_value()); 
            return &m_vtable; 
        }
        const auto* operator->() const noexcept { 
            assert(has_value()); 
            return &m_vtable;
        }

        auto& operator*() & noexcept { 
            assert(has_value()); 
            return m_vtable; 
        }
        const auto& operator*() const& noexcept { 
            assert(has_value()); 
            return m_vtable; 
        }
        auto&& operator*() && noexcept { 
            assert(has_value()); 
            return std::move(m_vtable); 
        }
        const auto&& operator*() const&& noexcept { 
            assert(has_value()); 
            return std::move(m_vtable); 
        }
      private:
        void copy_from(const any& other) {
            m_underlying = other.m_underlying;
            copy_vtable(other.m_vtable, m_vtable);
            update_vtable_context();
        }

        template <typename T, typename... Args>
        std::decay_t<T>* init_from(Args&&... args) noexcept(std::is_nothrow_constructible_v<std::decay_t<T>, Args&&...> && detail::storage::fits_sbo<std::decay_t<T>>()) {
            m_underlying.template emplace<T>(std::forward<Args>(args)...);
            return fill_vtable<T>();
        }

        template <typename T, typename... Args>
        any(init_tag<T>, Args&&... args) noexcept(std::is_nothrow_constructible_v<std::decay_t<T>, Args&&...> && detail::storage::fits_sbo<std::decay_t<T>>())
            : m_underlying(std::in_place_type<T>, std::forward<Args>(args)...) {
            fill_vtable<T>();
        }

        template <typename T, std::meta::info Member>
        void fill_vtable_fn(void* ptr) noexcept {
            constexpr static auto stripped_sig = template_arguments_of(dealias(type_of(Member)))[0];
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
                        constexpr static auto qualifiers = detail::qualifiers_of(full_sig);
                        if constexpr(
                            identifier_of(other_member) == identifier_of(Member) &&
                            dealias(remove_noexcept(type_of(other_member))) == full_sig
                        ) {
                            using vtable_function_type = [:substitute(^^vtable_function, {stripped_sig, ^^qualifiers}):];
                            m_vtable.[:Member:] = vtable_function_type::template make_fn<T, other_member>(ptr);
                            break;
                        }
                    }
                }
            }
        }

        template <typename T, std::meta::info Member>
        void fill_vtable_op(void* ptr) noexcept {
            constexpr static auto name        = identifier_of(Member);
            constexpr static auto stripped_sig = template_arguments_of(dealias(type_of(Member)))[0];

            template for (constexpr auto tag : {^^Tags...}) {
                if constexpr(template_of(tag) == ^^has_op) {
                    constexpr static auto tag_op   = [:template_arguments_of(tag)[0]:];
                    constexpr static auto tag_full_sig = template_arguments_of(tag)[1];
                    constexpr static auto qualifiers   = detail::qualifiers_of_target(tag_full_sig, ^^self);
                    constexpr static bool self_is_lhs  = remove_cvref(substitute(^^fn_arg_t, {tag_full_sig, std::meta::reflect_constant(0)})) == ^^self;

                    constexpr static auto after_remove_self = detail::remove_arg(tag_full_sig, ^^self);
                    constexpr static bool is_unary = extract<std::size_t>(substitute(^^fn_arg_count_v, {remove_fn_qualifiers(after_remove_self)})) == 0;
                    constexpr static bool has_other = !is_unary &&
                        remove_cvref(substitute(^^fn_arg_t, {tag_full_sig, std::meta::reflect_constant(self_is_lhs ? 1 : 0)})) == ^^other;


                    constexpr auto tag_name = std::string{"__rjk_"} + enum_to_string(tag_op) + (self_is_lhs ? "1" : "2");
                    if constexpr(tag_name == name) {
                        using vtable_function_type = [:substitute(^^vtable_function, {stripped_sig, ^^qualifiers}):];    
                        m_vtable.[:Member:] = vtable_function_type::template make_op<T, tag_op, self_is_lhs, has_other>(ptr);
                    }
                }
            }
        }

        template <typename T>
        std::decay_t<T>* fill_vtable() noexcept {
            auto* ptr = m_underlying.get();

            template for (constexpr auto member : vtable_members) {
                if constexpr(identifier_of(member) != copy_blocker_identifier) {
                    fill_vtable_fn<T, member>(ptr);
                    fill_vtable_op<T, member>(ptr);
                }
            }

            return static_cast<std::decay_t<T>*>(ptr);
        }

        void copy_vtable(const vtable& source, vtable& destination) noexcept {
            template for (constexpr auto member : vtable_members) {
                if constexpr(identifier_of(member) != copy_blocker_identifier) {
                    destination.[:member:] = source.[:member:];
                }
            }
        }

        void update_vtable_context() noexcept {
            auto* ptr = m_underlying.get();
            template for (constexpr auto member : vtable_members) {
                if constexpr(identifier_of(member) != copy_blocker_identifier) {
                    m_vtable.[:member:].obj = ptr;
                }
            }
        }
      private:
        enum struct overload_type {
            binary_lhs,
            binary_rhs,
            binary_both,
            unary
        };
        template <std::meta::operators Op, overload_type Type, typename Lhs, typename Rhs>
        consteval static bool satisfies_operator() {
            if constexpr(!has_operator_tag<Op, Tags...>()) {
                return false;
            }
            switch (Type) {
            case overload_type::binary_lhs:
                return std::is_same_v<std::remove_cvref_t<Lhs>, vtable> && !std::is_same_v<std::decay_t<Lhs>, std::decay_t<Rhs>>;
            case overload_type::binary_rhs: 
                return std::is_same_v<std::remove_cvref_t<Rhs>, vtable> && !std::is_same_v<std::decay_t<Lhs>, std::decay_t<Rhs>>;
            case overload_type::binary_both:
                return std::is_same_v<std::remove_cvref_t<Lhs>, vtable> && std::is_same_v<std::decay_t<Lhs>, std::decay_t<Rhs>>;
            case overload_type::unary:
                return std::is_same_v<std::remove_cvref_t<Lhs>, vtable>;
            }
            return false;
        }

        template <typename This1, typename This2> requires (satisfies_operator<op_plus, overload_type::binary_both, This1, This2>()) 
        friend decltype(auto) operator+(This1&& lhs, This2&& rhs) {
            return std::forward<This1>(lhs).__rjk_op_plus1(std::forward<This2>(rhs).__rjk_op_plus1.obj);
        }

        template <typename This> requires (satisfies_operator<op_plus, overload_type::unary, This, void>())
        friend decltype(auto) operator+(This&& operand) {
            return std::forward<This>(operand).__rjk_op_plus1();
        }

        template <typename This, typename R> requires (satisfies_operator<op_plus, overload_type::binary_lhs, This, R>()) 
        friend decltype(auto) operator+(This&& lhs, R&& rhs) {
            return std::forward<This>(lhs).__rjk_op_plus1(std::forward<R>(rhs));
        }

        template <typename L, typename This> requires (satisfies_operator<op_plus, overload_type::binary_rhs, L, This>()) 
        friend decltype(auto) operator+(L&& lhs, This&& rhs) {
            return std::forward<This>(rhs).__rjk_op_plus2(std::forward<L>(lhs));
        }
      private:
        detail::storage m_underlying{};
        vtable m_vtable{.__rjk_copy_blocker = detail::vtable_anchor{}};
    };    
}