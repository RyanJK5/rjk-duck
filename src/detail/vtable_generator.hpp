// clang-format off

#ifndef RJK_VTABLE_GENERATOR_HPP
#define RJK_VTABLE_GENERATOR_HPP

#include "detail/vtable_fn_maker.hpp"
#include "duck_tags.hpp"
#include "overload_resolution.hpp"

namespace rjk::detail {

template <typename DuckVtableGenerator>
class storage;

consteval std::string index_to_string(std::size_t index) {
    if (index == 0UZ) return std::string{'0'};
    std::string digits{};
    while (index > 0UZ) {
        digits += ('0' + index % 10UZ);

        index /= 10UZ;
    }
    std::ranges::reverse(digits);
    return digits;
}

consteval std::string index_to_slot_name(std::size_t index) {
    return "slot_" + index_to_string(index);
}

consteval std::string index_to_trait_name(std::size_t index) {
    return "to_trait_" + index_to_string(index);
}

template <is_trait... Traits>
struct vtable_generator {
    constexpr static auto ctx = std::meta::access_context::unprivileged();

    constexpr static std::array<std::meta::info, sizeof...(Traits)>
        traits{^^Traits...};

    constexpr static auto tags = define_static_array(traits
        | std::views::transform(members_to_tags)
        | std::views::join);

    constexpr static auto can_copy = std::ranges::contains(tags, ^^copy_tag);
    constexpr static auto is_mutable = (!std::is_const_v<Traits> || ...);

    using StorageType = storage<vtable_generator>;

    struct vtable;

    consteval static std::meta::info make_vtable_member(std::meta::info tag, std::string_view name) {
        const auto full_sig = template_arguments_of(tag)[1];
        if (template_of(tag) == ^^has_fn) {
            const auto erased_ptr_type =
                analyze_op_sig(template_arguments_of(tag)[1], op_parentheses)
                .erased_ptr_type;

            const auto sig = remove_fn_qualifiers(full_sig);
            const auto ptr_type = add_pointer(prepend_arg(
                erased_ptr_type, sig));
            return data_member_spec(ptr_type, {
                .name = name
            });
        }
        else if (template_of(tag) == ^^has_op) {
            const auto [_, _, after_remove_self,
                erased_ptr_type] = analyze_op_tag(tag);

            const auto sig = remove_fn_qualifiers(after_remove_self);
            const auto ptr_type = add_pointer(prepend_arg(
                erased_ptr_type, sig));
            return data_member_spec(ptr_type, {
                .name = name
            });
        }
    }

    consteval {
        std::vector<std::meta::info> members{
#ifdef __cpp_rtti
            data_member_spec(^^const std::type_info*, {.name = "typeid_of"}),
#endif
            data_member_spec(^^void(*)(StorageType&) noexcept, {.name = "destroy"}),
            data_member_spec(^^void(*)(void*, StorageType&) noexcept, {.name = "move"})
        };
        if constexpr (can_copy) {
            members.push_back(data_member_spec(^^void(*)(const void*, StorageType&), {.name = "copy"}));
        }
        if constexpr (is_mutable) {
            members.push_back(data_member_spec(
                ^^const typename vtable_generator<const Traits...>::vtable*,
                {.name = "to_const"}
            ));
        }

        [[maybe_unused]] std::size_t index{};
        template for (constexpr auto trait_index : std::views::indices(traits.size())) {
            constexpr static auto trait = traits[trait_index];
            constexpr static auto trait_table = substitute(
                ^^::rjk::detail::vtable_generator, {trait});

            members.push_back(data_member_spec(
                ^^const typename [:trait_table:]::vtable*,
                {.name = index_to_trait_name(trait_index)}
            ));

            for (const auto tag : members_to_tags(trait)) {
                if (tag == ^^copy_tag) {
                    index++;
                    continue;
                }

                members.push_back(make_vtable_member(tag, index_to_slot_name(index)));
                index++;
            }
        }

        define_aggregate(^^vtable, members);
    }

    constexpr static auto vtable_members = define_static_array(
        nonstatic_data_members_of(^^vtable, ctx));

    consteval static auto find_trait(std::meta::info trait) {
        struct ret_value {
            const std::meta::info* trait_loc;
            bool should_constify;
        };
        if (const auto with_const = std::ranges::find(traits, trait);
                    with_const != traits.end()) {
            return ret_value{with_const, false};
                    }
        if (const auto without_const =
                std::ranges::find(traits, remove_const(trait));
                without_const != traits.end()) {
            return ret_value{without_const, true};
                }
        display_error("trait not found");
    }

    template <is_trait Trait> requires
        ((std::same_as<Traits, Trait> || ...) ||
        (std::same_as<Traits, std::remove_const_t<Trait>> || ...))
    constexpr static const vtable_generator<Trait>::vtable* convert(const vtable* table) {
        constexpr static auto trait_info = find_trait(^^Trait);

        constexpr static auto member = *std::ranges::find_if(
            vtable_members,
            [](auto member) {
                return identifier_of(member) == index_to_trait_name(
                    std::ranges::distance(traits.begin(), trait_info.trait_loc));
            }
        );

        if constexpr (trait_info.should_constify) {
            return table->[:member:]->to_const;
        } else {
            return table->[:member:];
        }
    }

    consteval static std::meta::info find_trait_for_tag(std::meta::info tag) {
        return *std::ranges::find_if(traits, [tag](auto trait) {
            return std::ranges::contains(members_to_tags(trait), tag);
        });
    }

    // The special functions, like move, copy, and destroy, are defined in
    // detail/storage.hpp.
    template <typename T>
    consteval static void set_storage_functions(vtable& static_vtable);

    template <typename T>
    consteval static vtable make_vtable();

    // Generates a static_vtable with the correct member functions for T.
    template <typename T>
    constexpr static auto static_vtable_for = make_vtable<T>();
};

// TODO: Remove once GCC fixes bug
template <std::meta::info... Traits>
using vtable_generator_meta = vtable_generator<typename [:Traits:]...>;

template <is_trait... Traits>
template <typename T>
consteval auto vtable_generator<Traits...>::make_vtable() -> vtable {
    vtable table{};
#ifdef __cpp_rtti
    table.typeid_of = &typeid(T);
#endif
    if constexpr (is_mutable) {
        table.to_const = &vtable_generator<const Traits...>::template
            static_vtable_for<T>;
    }
    set_storage_functions<T>(table);

    template for (constexpr auto index : std::views::indices(traits.size())) {
        constexpr static auto converter = *std::ranges::find_if(
            vtable_members,
            [](auto member) { return identifier_of(member) == index_to_trait_name(index); }
        );
        table.[:converter:] = &vtable_generator<Traits...[index]>::template
            static_vtable_for<T>;
    }

    template for (constexpr auto index : std::views::indices(tags.size())) {
        constexpr static auto tag = tags[index];

        if constexpr (tag != ^^copy_tag) {
            constexpr static auto slot = *std::ranges::find_if(
                vtable_members,
                [](auto member) { return identifier_of(member) == index_to_slot_name(index); }
            );

            if constexpr (has_template_arguments(tag) && template_of(tag) == ^^has_fn) {
                constexpr static auto member_name = template_arguments_of(tag)[0];
                constexpr static auto full_sig    = template_arguments_of(tag)[1];
                constexpr static auto qualifiers  = qualifiers_of(full_sig);

                constexpr static auto sig = remove_fn_qualifiers(full_sig);

                constexpr static auto fn_maker = std::invoke([] {
                    const auto impl = find_impl_specialization(^^T, find_trait_for_tag(tag),
                            std::string_view{[:member_name:]}, full_sig, true);
                    if (impl.has_value()) {
                        return substitute(^^vtable_fn_maker_meta, {
                            reflect_constant(sig),
                            std::meta::reflect_constant(qualifiers),
                            reflect_constant(^^T),
                            reflect_constant(impl.value())
                        });
                    }

                    const auto overload_set_t = make_set(
                        decay(^^T),
                        std::string_view{[:member_name:]});

                    return substitute(^^vtable_fn_maker_meta, {
                        reflect_constant(sig),
                        std::meta::reflect_constant(qualifiers),
                        reflect_constant(^^T),
                        reflect_constant(overload_set_t)
                    });
                });

                table.[:slot:] = [:fn_maker:]::make();
            }
            else if constexpr (has_template_arguments(tag) && template_of(tag) == ^^has_op) {
                constexpr static auto [op_kind, qualifiers, after_remove_self, _]
                    = analyze_op_tag(tag);
                constexpr static auto tag_op = template_arguments_of(tag)[0];

                constexpr static auto sig = remove_fn_qualifiers(after_remove_self);

                constexpr static auto op_maker = substitute(^^vtable_op_maker_meta, {
                    reflect_constant(sig),
                    std::meta::reflect_constant(qualifiers),
                    tag_op,
                    std::meta::reflect_constant(op_kind),
                    reflect_constant(^^T)
                });

                table.[:slot:] = [:op_maker:]::make();
            }
        }
    }

    return table;
}

}

#endif
