#ifndef RJK_DUCK_BASE_HPP
#define RJK_DUCK_BASE_HPP

#include <meta>

#include "duck_tags.hpp"
#include "vtable_fn_maker.hpp"
#include "substitute_fn_traits.hpp"

namespace rjk {
template <duck_tag... Tags>
class duck;
namespace detail {

template <duck_tag... Tags>
class storage;

template <duck_tag... Tags>
class duck_base {
public:
    friend class storage<Tags...>;
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
        using StorageType = storage<Tags...>;
        std::vector<std::meta::info> members{
            data_member_spec(^^void(*)(StorageType&) noexcept, {.name = "destroy"}),
            data_member_spec(^^void(*)(StorageType&, StorageType&) noexcept, {.name = "move"}),
            data_member_spec(^^void(*)(const StorageType&, StorageType&), {.name = "copy"}),
        };

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
    consteval static void set_storage_functions(static_duck_vtable& static_vtable);

    template <typename T>
    constexpr static auto static_vtable_for = [] {
        static_duck_vtable table{};
        set_storage_functions<T>(table);

        constexpr static auto slots = define_static_array(
            nonstatic_data_members_of(^^duck_base<Tags...>::static_duck_vtable, ctx)
            | std::views::drop(3)
        );

        [[maybe_unused]] constexpr static
            std::array<std::meta::info, sizeof...(Tags)> tags{^^Tags...};

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

        duck<Tags...>& trace_to_duck();
        const duck<Tags...>& trace_to_duck() const;

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
        [[maybe_unused]] constexpr static
            std::array<std::meta::info, sizeof...(Tags)> tags{^^Tags...};
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
}

#endif //RJK_DUCK_BASE_HPP
