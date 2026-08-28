// clang-format off

#ifndef RJK_VTABLE_GENERATOR_HPP
#define RJK_VTABLE_GENERATOR_HPP

#include "detail/vtable_fn_maker.hpp"
#include "duck_tags.hpp"
#include "overload_resolution.hpp"

namespace rjk::detail {

template <typename DuckVtableGenerator>
class storage;

consteval std::string index_to_slot_name(
    std::integral auto trait_index, std::integral auto member_index) {
    return "slot_" + index_to_string(trait_index) +
           "_" + index_to_string(member_index);
}

consteval std::string index_to_trait_name(std::integral auto index, bool owning) {
    return std::string{"to_trait_"} + (owning ? "owning_" : "view_") + index_to_string(index);
}
// slot_0_1

consteval std::size_t extract_trait_index(std::string_view converter) {
    const auto itr = std::ranges::find_if(converter, [](char c) {
        return std::isdigit(c);
    });
    return string_to_index(std::string_view{itr, converter.end()});
}

struct index_pair {
    std::size_t trait_index;
    std::size_t member_index;
};

consteval index_pair extract_indices(std::string_view slot) {
    const auto lastUnderscore = slot.find_last_of('_');
    return {
        .trait_index = string_to_index(slot.substr(5uz, lastUnderscore - 5uz)),
        .member_index = string_to_index(slot.substr(lastUnderscore + 1uz))
    };
}

template <typename... Traits>
struct vtable_generator {
    constexpr static auto ctx = std::meta::access_context::unprivileged();

    constexpr static auto traits = define_static_array(std::vector<std::meta::info>{^^Traits...});

    constexpr static auto can_copy = (std::ranges::any_of(members_for<Traits>, [](auto member) {
        return is_copy_constructor(member);
    }) || ...);
    constexpr static auto is_mutable = (!std::is_const_v<Traits> || ...);

    struct vtable;

    consteval static std::meta::info make_vtable_member(std::meta::info member, std::string_view name) {
        const auto erased_ptr_type = is_const(member) ?
            ^^const void* : ^^void*;
        const auto sig = remove_fn_qualifiers(type_of(member));
        const auto ptr_type = add_pointer(prepend_arg(
            erased_ptr_type, sig));
        return data_member_spec(ptr_type, {
            .name = name
        });
    }

    consteval {
        using allocator = storage<vtable_generator>::allocator_type;

        std::vector<std::meta::info> members{
#ifdef __cpp_rtti
            data_member_spec(^^const std::type_info*, {.name = "typeid_of"}),
#endif
            data_member_spec(^^void(*)(void*, const allocator&) noexcept, {.name = "destroy"}),
            data_member_spec(^^void*(*)(void*, std::byte*), {.name = "fast_move"}),
            data_member_spec(^^void*(*)(void*, std::byte*, const allocator&), {.name = "slow_move"}),
        };
        if constexpr (can_copy) {
            members.push_back(data_member_spec(^^void*(*)(const void*, std::byte*, const allocator&), {.name = "copy"}));
        }
        if constexpr (is_mutable) {
            using generator = ::rjk::detail::vtable_generator<const Traits...>;
            members.push_back(data_member_spec(
                ^^const typename generator::vtable*,
                {.name = "to_const_owning"}
            ));
            members.push_back(data_member_spec(
                ^^const typename generator::vtable*,
                {.name = "to_const_view"}
            ));
        }

        template for (constexpr auto trait_index : std::views::indices(traits.size())) {
            if constexpr (sizeof...(Traits) > 1uz) {
                using generator = ::rjk::detail::vtable_generator<Traits...[trait_index]>;

                const auto trait_table = ^^typename generator::vtable;
                const auto table_pointer = add_pointer(add_const(trait_table));

                members.push_back(data_member_spec(table_pointer,
                    {.name = index_to_trait_name(trait_index, false)}
                ));
                members.push_back(data_member_spec(table_pointer,
                    {.name = index_to_trait_name(trait_index, true)}
                ));
            }

            for (const auto [index, member] : std::views::enumerate(members_for<Traits...[trait_index]>)) {
                if (is_user_provided(member)) {
                    const auto str = index_to_slot_name(trait_index, index);
                    members.push_back(make_vtable_member(member, str));
                }
            }
        }

        define_aggregate(^^vtable, members);
    }

    constexpr static auto vtable_members = define_static_array(
        nonstatic_data_members_of(^^vtable, ctx));

    template <typename Trait, bool Owning> requires
        ((std::same_as<Traits, Trait> || ...) ||
        (std::same_as<Traits, std::remove_const_t<Trait>> || ...))
    constexpr static const vtable_generator<Trait>::vtable* convert(const vtable* table) {
        constexpr static auto trait_itr = std::ranges::find_if(traits, [](auto trait) {
            return trait == ^^Trait || add_const(trait) == ^^Trait;
        });
        constexpr static auto index = std::ranges::distance(traits.begin(), trait_itr);
        constexpr static auto should_constify = is_const(*trait_itr) != is_const(^^Trait);

        constexpr static auto member = *std::ranges::find_if(vtable_members,
            [](auto member) {
                return identifier_of(member) == index_to_trait_name(index, Owning);
            }
        );

        if constexpr (should_constify) {
            if constexpr (Owning) {
                return table->[:member:]->to_const_owning;
            } else {
                return table->[:member:]->to_const_view;
            }
        } else {
            return table->[:member:];
        }
    }

    // The special functions, like move, copy, and destroy, are defined in
    // detail/storage.hpp.
    template <typename T>
    consteval static void set_storage_functions(vtable& static_vtable);

    template <typename T, bool Owning>
    consteval static vtable make_vtable();

    // Generates a static_vtable with the correct member functions for T.
    template <typename T>
    constexpr static auto view_vtable = make_vtable<T, false>();

    template <typename T>
    constexpr static auto owning_vtable = make_vtable<T, true>();

    constexpr static auto null_vtable = [] {
        using allocator = storage<vtable_generator>::allocator_type;
        vtable v{};

        v.destroy = [](void*, const allocator&) noexcept -> void {};
        v.fast_move = [](void*, std::byte*) noexcept -> void* {
            return nullptr;
        };
        v.slow_move = [](void*, std::byte*, const allocator&) noexcept -> void* {
            return nullptr;
        };
        if constexpr (can_copy) {
            v.copy = [](const void*, std::byte*, const allocator&) noexcept -> void* {
                return nullptr;
            };
        }

        return v;
    }();

    template <typename T>
    consteval static auto get_fn_maker(std::meta::info trait, std::meta::info member) {
        const auto qualifiers = qualifiers_of(member);
        const auto full_sig = type_of(member);
        const auto sig = remove_fn_qualifiers(full_sig);
        const auto member_name = identifier_of(member);

        const auto impl = find_impl_specialization(^^T, trait,
                member_name, full_sig);
        if (impl.has_value()) {
            return substitute(^^vtable_fn_maker, {
                sig,
                std::meta::reflect_constant(qualifiers),
                ^^T,
                impl.value()
            });
        }

        const auto overload_set_t = make_set(decay(^^T),
        {.identifier = fixed_string{member_name},
            .param_count = parameters_of(full_sig).size()});

        return substitute(^^vtable_fn_maker, {
            sig, std::meta::reflect_constant(qualifiers), ^^T, overload_set_t
        });
    }

    template <typename T>
    consteval static auto get_op_maker(std::meta::info member) {
        const auto op = operator_of(member);
        const auto sig = remove_fn_qualifiers(type_of(member));
        const auto qualifiers = qualifiers_of(member);
        const auto op_kind = op_kind_of(member);

        return substitute(^^vtable_op_maker, {
            sig, std::meta::reflect_constant(qualifiers), reflect_constant(op),
            std::meta::reflect_constant(op_kind), ^^T
        });
    }
};

template <typename... Traits>
template <typename T, bool Owning>
consteval auto vtable_generator<Traits...>::make_vtable() -> vtable {
    vtable table{};
#ifdef __cpp_rtti
    table.typeid_of = &typeid(T);
#endif
    if constexpr (is_mutable) {
        table.to_const_view = &vtable_generator<const Traits...>::template view_vtable<T>;
        table.to_const_owning = &vtable_generator<const Traits...>::template owning_vtable<T>;
    }

    if constexpr (Owning) {
        set_storage_functions<T>(table);
    }

    constexpr static auto members = define_static_array(
        nonstatic_data_members_of(^^vtable, ctx)
        | std::views::drop_while([](auto member) {
            return !identifier_of(member).starts_with("to_trait")
                && !identifier_of(member).starts_with("slot");
        }));

    template for (constexpr auto slot : members) {
        constexpr static auto id = identifier_of(slot);
        if constexpr (id[0] == 't') {
            constexpr static auto index = extract_trait_index(identifier_of(slot));
            if constexpr (id.find("owning") != std::string_view::npos) {
                table.[: slot :] = &vtable_generator<Traits...[index]>::template owning_vtable<T>;
            } else {
                table.[: slot :] = &vtable_generator<Traits...[index]>::template view_vtable<T>;
            }
        } else {
            constexpr static auto [trait_index, member_index] = extract_indices(identifier_of(slot));
            constexpr static auto member = members_for<Traits...[trait_index]>[member_index];

            if constexpr (is_operator_function(member)) {
                constexpr static auto op_maker = get_op_maker<T>(member);
                table.[: slot :] = [:op_maker:]::make();
            } else if constexpr (!is_copy_constructor(member)){
                constexpr static auto fn_maker = get_fn_maker<T>(traits[trait_index], member);
                table.[: slot :] = [:fn_maker:]::make();
            }
        }
    }

    return table;
}

consteval std::meta::info make_vtable_generator(std::vector<std::meta::info> traits) {
    std::ranges::sort(traits, [](auto a, auto b) {
        return std::is_lt(compare_meta_info(a, b));
    });
    return substitute(^^vtable_generator, traits);
}

consteval std::meta::info make_vtable_generator(std::meta::info duck_type) {
    return make_vtable_generator(template_arguments_of(decay(duck_type)));
}

}

#endif
