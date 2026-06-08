//
// Created by Ryan on 6/6/2026.
//

#ifndef RJK_DUCK_VTABLE_HPP
#define RJK_DUCK_VTABLE_HPP

#include "detail/vtable_fn_maker.hpp"
#include "duck_tags.hpp"

namespace rjk::detail {

template <typename DuckVtableGenerator>
class storage;

// Converts '0' -> '_rjk__slot_0'
consteval static std::string index_to_slot_name(std::size_t index) {
    std::string result{"_rjk__slot_"};
    if (index == 0UZ) return result + '0';
    std::string digits{};
    while (index > 0UZ) {
        digits += ('0' + index % 10UZ);
        index /= 10UZ;
    }
    std::ranges::reverse(digits);
    return result + digits;
}

template <typename DuckType, typename DuckViewType, duck_tag... Tags>
struct duck_vtable_generator {
    struct static_duck_vtable;

    constexpr static bool can_copy = (std::same_as<Tags, copy_tag> || ...);

    constexpr static auto ctx = std::meta::access_context::unprivileged();

    // Defines the static_vtable struct. This contains all the function
    // pointers for a given duck, and accepts a type-erased void* as the first
    // argument, representing the object held within a duck.
    consteval {
        using StorageType = storage<duck_vtable_generator>;

        std::vector<std::meta::info> members{ // special member functions
            data_member_spec(^^void(*)(StorageType&) noexcept, {.name = "destroy"}),
            data_member_spec(^^void(*)(StorageType&, StorageType&) noexcept, {.name = "move"})
        };
        if constexpr (can_copy) {
            members.push_back(
                data_member_spec(^^void(*)(const StorageType&, StorageType&), {.name = "copy"}));
        }

        auto index = 0UZ;
        template for (constexpr auto tag : {^^Tags...}) {
            if constexpr (tag != ^^copy_tag) {
                constexpr static auto full_sig = template_arguments_of(tag)[1];

                if constexpr (template_of(tag) == ^^has_fn) {
                    constexpr static auto erased_ptr_type =
                        analyze_op_sig(template_arguments_of(tag)[1], op_parentheses)
                        .erased_ptr_type;

                    constexpr static auto sig = remove_noexcept(
                        remove_fn_qualifiers(full_sig));
                    constexpr static auto ptr_type = substitute(^^fn_to_ptr_t,
                        {substitute(^^detail::prepend_arg_t, {erased_ptr_type, sig})});
                    members.push_back(data_member_spec(ptr_type, {
                        .name = index_to_slot_name(index)
                    }));
                }
                else if constexpr (template_of(tag) == ^^has_op) {
                    constexpr static auto [_, qualifiers, after_remove_self,
                        erased_ptr_type] = analyze_op_tag(tag);

                    constexpr static auto sig = normalized_sig(after_remove_self,
                        ^^DuckType, ^^DuckViewType);
                    constexpr static auto ptr_type = substitute(^^fn_to_ptr_t,
                        {substitute(^^detail::prepend_arg_t, {erased_ptr_type, sig})});
                    members.push_back(data_member_spec(ptr_type, {
                        .name = index_to_slot_name(index)
                    }));
                }
                index++;
            }
        }

        define_aggregate(^^static_duck_vtable, members);
    }

    // The special functions, like move, copy, and destroy, are defined in
    // detail/storage.hpp.
    template <typename T>
    consteval static void set_storage_functions(static_duck_vtable& static_vtable);

    // Generates a static_vtable with the correct member functions for T.
    template <typename T>
    constexpr static auto static_vtable_for = std::invoke([] {
        static_duck_vtable table{};
        set_storage_functions<T>(table);

        constexpr static auto slots = define_static_array(
            nonstatic_data_members_of(^^static_duck_vtable, ctx)
            | std::views::drop(can_copy ? 3 : 2) // drop special members
        );

        if constexpr(sizeof...(Tags) == 0) {
            return table;
        } else {
            constexpr static std::array tags{^^Tags...};

            template for (constexpr auto index : std::views::indices(sizeof...(Tags))) {
                constexpr static auto tag = tags[index];
                if constexpr (tag != ^^copy_tag) {
                    if constexpr (template_of(tag) == ^^has_fn) {
                        constexpr static auto member_name = template_arguments_of(tag)[0];
                        constexpr static auto full_sig    = template_arguments_of(tag)[1];
                        constexpr static auto qualifiers  = qualifiers_of(full_sig);

                        constexpr static auto T_member = std::invoke([] -> std::meta::info {
                            template for (constexpr auto m : define_static_array(members_of(^^T, ctx))) {
                                if constexpr (has_identifier(m) && is_function(m)) {
                                    if constexpr (identifier_of(m) == std::string_view{[:member_name:]}) {
                                        if constexpr (dealias(remove_noexcept(type_of(m))) == remove_noexcept(full_sig)) {
                                            return m;
                                        }
                                    }
                                }
                            }
                            throw std::logic_error("Could not find member");
                        });

                        constexpr static auto sig = remove_noexcept(
                            remove_fn_qualifiers(full_sig));
                        constexpr static auto fn_maker = substitute(^^vtable_fn_maker,
                            {sig, ^^qualifiers, ^^T_member, ^^T});

                        table.[:slots[index]:] = [:fn_maker:]::make();
                    }
                    else if constexpr (template_of(tag) == ^^has_op) {
                        constexpr static auto [op_kind, qualifiers, after_remove_self, _]
                            = analyze_op_tag(tag);
                        constexpr static auto tag_op = template_arguments_of(tag)[0];

                        constexpr static auto sig = normalized_sig(after_remove_self,
                            ^^DuckType, ^^DuckViewType);

                        constexpr static auto op_maker = substitute(^^vtable_op_maker,
                            {sig, std::meta::reflect_constant(qualifiers),
                                tag_op, std::meta::reflect_constant(op_kind), ^^T});

                        table.[:slots[index]:] = [:op_maker:]::make();
                    }
                }
            }
        }

        return table;
    });
};

}

#endif //RJK_DUCK_VTABLE_HPP
