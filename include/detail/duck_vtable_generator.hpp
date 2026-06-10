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

// Converts '0' -> '_rjk_slot_0'
consteval static std::string index_to_slot_name(std::size_t index) {
    std::string result{"_rjk_slot_"};
    if (index == 0UZ) return result + '0';
    std::string digits{};
    while (index > 0UZ) {
        digits += ('0' + index % 10UZ);
        index /= 10UZ;
    }
    std::ranges::reverse(digits);
    return result + digits;
}

template <is_trait Trait>
struct trait_vtable_impl {
    struct type;

    // Defines each trait_vtable struct. This contains all the function
    // pointers for a given trait, and accepts a type-erased void* as the first
    // argument, representing the object held within a duck.
    consteval {
        std::vector<std::meta::info> members{};
        std::size_t index{};
        for (const auto tag : members_to_tags(^^Trait)) {
            if (tag == ^^copy_tag) {
                index++;
                continue;
            }

            const auto full_sig = template_arguments_of(tag)[1];
            if (template_of(tag) == ^^has_fn) {
                const auto erased_ptr_type =
                    analyze_op_sig(template_arguments_of(tag)[1], op_parentheses)
                    .erased_ptr_type;

                const auto sig = remove_noexcept(
                    remove_fn_qualifiers(full_sig));
                const auto ptr_type = substitute(^^fn_to_ptr_t,
                    {substitute(^^detail::prepend_arg_t, {erased_ptr_type, sig})});
                members.push_back(data_member_spec(ptr_type, {
                    .name = index_to_slot_name(index)
                }));
            }
            else if (template_of(tag) == ^^has_op) {
                const auto [_, qualifiers, after_remove_self,
                    erased_ptr_type] = analyze_op_tag(tag);

                const auto sig = normalized_sig(after_remove_self);
                const auto ptr_type = substitute(^^fn_to_ptr_t,
                    {substitute(^^detail::prepend_arg_t, {erased_ptr_type, sig})});
                members.push_back(data_member_spec(ptr_type, {
                    .name = index_to_slot_name(index)
                }));
            }

            index++;
        }

        define_aggregate(^^type, members);
    }
};

template <is_trait... Traits>
struct duck_vtable_generator {
    constexpr static std::array<std::meta::info, sizeof...(Traits)>
        traits{^^Traits...};

    constexpr static auto tags = define_static_array(traits
        | std::views::transform(members_to_tags)
        | std::views::join);

    constexpr static auto can_copy = std::ranges::contains(tags, ^^copy_tag);

    template <is_trait Trait>
    using trait_vtable = trait_vtable_impl<Trait>::type;

    using StorageType = storage<duck_vtable_generator>;

    constexpr static auto ctx = std::meta::access_context::unprivileged();

    struct vtable : trait_vtable<Traits>... {
        void (*destroy) (StorageType&) noexcept;
        void (*move) (StorageType&, StorageType&) noexcept;
        void (*copy) (const StorageType&, StorageType&);

        consteval static std::meta::info slot(std::size_t index) {
            std::size_t local_index = index;
            std::meta::info owning_trait{};
            for (const auto trait : traits) {
                const auto tag_count = members_to_tags(trait).size();
                if (local_index < tag_count) {
                    owning_trait = trait;
                    break;
                }
                local_index -= tag_count;
            }

            const auto base_members = nonstatic_data_members_of(
                substitute(^^trait_vtable, {owning_trait}), ctx);

            return *std::ranges::find_if(base_members,
                [local_index](auto member) {
                    return identifier_of(member) == index_to_slot_name(local_index);
                });
        }
    };

    // The special functions, like move, copy, and destroy, are defined in
    // detail/storage.hpp.
    template <typename T>
    consteval static void set_storage_functions(vtable& static_vtable);

    // Generates a static_vtable with the correct member functions for T.
    template <typename T>
    constexpr static auto static_vtable_for = std::invoke([] {
        vtable table{};
        set_storage_functions<T>(table);

        template for (constexpr auto index : std::views::indices(tags.size())) {
            constexpr static auto tag = tags[index];

            if constexpr (tag != ^^copy_tag) {
                constexpr static auto slot = vtable::slot(index);

                if constexpr (has_template_arguments(tag) && template_of(tag) == ^^has_fn) {
                    constexpr static auto member_name = template_arguments_of(tag)[0];
                    constexpr static auto full_sig    = template_arguments_of(tag)[1];
                    constexpr static auto qualifiers  = qualifiers_of(full_sig);

                    constexpr static auto T_member = std::invoke([] consteval -> std::meta::info {
                        for (const auto m : members_of(^^T, ctx)) {
                            if (has_identifier(m) && is_function(m) &&
                                identifier_of(m) == std::string_view{[:member_name:]} &&
                                dealias(remove_noexcept(type_of(m))) == remove_noexcept(full_sig)) {
                                return m;
                            }
                        }
                        throw std::logic_error{"Could not find member"};
                    });

                    constexpr static auto sig = remove_noexcept(
                        remove_fn_qualifiers(full_sig));
                    constexpr static auto fn_maker = substitute(^^vtable_fn_maker,
                        {sig, ^^qualifiers, ^^T_member, ^^T});

                    table.[:slot:] = [:fn_maker:]::make();
                }
                else if constexpr (has_template_arguments(tag) && template_of(tag) == ^^has_op) {
                    constexpr static auto [op_kind, qualifiers, after_remove_self, _]
                        = analyze_op_tag(tag);
                    constexpr static auto tag_op = template_arguments_of(tag)[0];

                    constexpr static auto sig = normalized_sig(after_remove_self);

                    constexpr static auto op_maker = substitute(^^vtable_op_maker,
                        {sig, std::meta::reflect_constant(qualifiers),
                            tag_op, std::meta::reflect_constant(op_kind), ^^T});

                    table.[:slot:] = [:op_maker:]::make();
                }
            }
        }

        return table;
    });
};

}

#endif //RJK_DUCK_VTABLE_HPP
