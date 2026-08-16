// clang-format off

#ifndef RJK_VTABLE_GENERATOR_HPP
#define RJK_VTABLE_GENERATOR_HPP

#include "detail/vtable_fn_maker.hpp"
#include "duck_tags.hpp"
#include "overload_resolution.hpp"

namespace rjk::detail {

template <typename DuckVtableGenerator>
class storage;

template <std::integral IndexT>
consteval std::string index_to_string(IndexT index) {
    constexpr static IndexT zero{0};
    constexpr static IndexT ten{10};

    if (index == zero) return std::string{'0'};
    std::string digits{};
    while (index > zero) {

        digits += ('0' + static_cast<char>(index % ten));
        index /= ten;
    }
    std::ranges::reverse(digits);
    return digits;
}

consteval std::size_t string_to_index(std::string_view str) {
    auto result{};
    for (const auto c : std::views::reverse(str)) {
        result *= 10uz;
        result += static_cast<std::size_t>(c - '0');
    }
    return result;
}

consteval std::string index_to_slot_name(
    std::size_t trait_index, std::size_t member_index) {
    return "slot_" + index_to_string(trait_index) +
           "_" + index_to_string(member_index);
}

consteval std::string index_to_trait_name(std::integral auto index) {
    return "to_trait_" + index_to_string(index);
}
// slot_0_1

struct index_pair {
    std::size_t trait_index;
    std::size_t member_index;
};

consteval std::size_t extract_trait_index(std::string_view converter) {
    return string_to_index(slot.substr(10uz));
}

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

    constexpr static auto can_copy = std::ranges::any_of(traits, [](auto trait) {
        return std::ranges::any_of(members_of(trait, ctx), [](auto member) {
            return is_user_provided(member) && is_defaulted(member) && is_copy_constructor(member);
        });
    });
    constexpr static auto is_mutable = (!std::is_const_v<Traits> || ...);


    using storage_t = storage<vtable_generator>;

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
        std::vector<std::meta::info> members{
#ifdef __cpp_rtti
            data_member_spec(^^const std::type_info*, {.name = "typeid_of"}),
#endif
            data_member_spec(^^void(*)(storage_t&) noexcept, {.name = "destroy"}),
            data_member_spec(^^void(*)(void*, typename storage_t::allocator_type&, storage_t&), {.name = "move_construct"}),
            data_member_spec(^^void(*)(void*, storage_t&), {.name = "fresh_move_construct"}),
            data_member_spec(^^void(*)(storage_t&, storage_t&), {.name = "move_assign"})
        };
        if constexpr (can_copy) {
            members.push_back(data_member_spec(^^void(*)(const void*, storage_t&), {.name = "copy"}));
        }
        if constexpr (is_mutable) {
            members.push_back(data_member_spec(
                ^^const typename vtable_generator<const Traits...>::vtable*,
                {.name = "to_const"}
            ));
        }

        for (const auto [trait_index, trait] : std::views::enumerate(traits)) {
            const auto generator = substitute(
                ^^::rjk::detail::vtable_generator, {trait});

            const auto gen_members = members_of(generator, ctx);
            const auto trait_table = *std::ranges::find(gen_members, [](auto member) {
                    return identifier_of(member) == "vtable";
                });
            const auto table_pointer = add_pointer(add_const(trait_table));

            members.push_back(data_member_spec(table_pointer,
                {.name = index_to_trait_name(trait_index)}
            ));

            for (const auto [index, member] : std::views::enumerate(all_trait_members(trait))) {
                const auto str = index_to_slot_name(trait_index, index);
                const auto name = members.push_back(make_vtable_member(member, str));
            }
        }

        define_aggregate(^^vtable, members);
    }

    constexpr static auto vtable_members = define_static_array(
        nonstatic_data_members_of(^^vtable, ctx));

    template <typename Trait> requires
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
                return identifier_of(member) == index_to_trait_name(index);
            }
        );

        if constexpr (trait_info.should_constify) {
            return table->[:member:]->to_const;
        } else {
            return table->[:member:];
        }
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

    constexpr static auto null_vtable = [] {
        vtable v{};

        v.destroy = [](storage_t&) noexcept {};
        v.move_construct = [](void*, typename storage_t::allocator_type&, storage_t&) noexcept {};
        v.fresh_move_construct = [](void*, storage_t&) noexcept {};
        v.move_assign = [](storage_t&, storage_t&) noexcept {};
        if constexpr (can_copy) {
            v.copy = [](const void*, storage_t&) noexcept {};
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
        {.identifier = member_name,
            .param_count = parameters_of(full_sig).size()});

        return substitute(^^vtable_fn_maker, {
            sig, std::meta::reflect_constant(qualifiers), ^^T, overload_set_t
        });
    }

    template <typename T>
    consteval auto get_op_maker(std::meta::info member) {
        const auto op = operator_of(member);
        const auto sig = remove_fn_qualifiers(type_of(member));
        const auto qualifiers = qualifiers_of(member);
        const auto op_kind = op_kind_of(member);

        return substitute(^^vtable_op_maker, {
            sig, std::meta::reflect_constant(qualifiers), op,
            std::meta::reflect_constant(op_kind), ^^T
        });
    }
};

template <typename... Traits>
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

    constexpr static auto members = define_static_array(
        nonstatic_data_members_of(^^vtable, ctx)
        | std::views::drop_while([](auto member) {
            return !identifier_of(member).starts_with("to_trait");
        }));

    template for (constexpr auto slot : members) {
        if constexpr (identifier_of(slot)[0] == 't') {
            constexpr static auto index = extract_trait_index(identifier_of(slot));
            table.[: slot :] =
                &vtable_generator<Traits...[index]>::template static_vtable_for<T>;
        } else {
            constexpr static auto [trait_index, member_index] = extract_indices(identifier_of(slot));
            constexpr static auto member = members_for<Traits...[trait_index]>[member_index];

            if constexpr (is_operator_function(member)) {
                constexpr static auto op_maker = get_op_maker<T>(member);
                table.[: slot :] = [:op_maker:]::make();
            } else {
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
