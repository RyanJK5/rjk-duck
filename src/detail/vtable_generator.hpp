// clang-format off

#ifndef RJK_VTABLE_GENERATOR_HPP
#define RJK_VTABLE_GENERATOR_HPP

#include <optional>

#include "detail/vtable_fn_maker.hpp"
#include "duck_tags.hpp"
#include "overload_resolution.hpp"
#include "detail/perf_options.hpp"

#ifdef __cpp_rtti
#include <typeinfo>
#endif

namespace rjk::detail {

// Identifies the type a table was made for: its type_info with RTTI, otherwise
// a tag unique to it. The null tables use void's.
#ifdef __cpp_rtti
using type_id_t = std::type_info;

template <typename T>
constexpr const type_id_t& type_id_of() noexcept { return typeid(T); }

// Compares type_info rather than table addresses, which can differ for the
// same type across shared libraries.
template <typename T>
constexpr bool id_matches(const type_id_t& id) noexcept { return id == typeid(T); }
#else
struct type_id_t {};

template <typename T>
inline constexpr type_id_t type_tag{};

template <typename T>
constexpr const type_id_t& type_id_of() noexcept { return type_tag<T>; }

template <typename T>
constexpr bool id_matches(const type_id_t& id) noexcept { return &id == &type_tag<T>; }
#endif

consteval std::string index_to_slot_name(
    std::integral auto trait_index, std::integral auto member_index) {
    return "slot_" + index_to_string(trait_index) +
           "_" + index_to_string(member_index);
}
// slot_0_1

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

// Names the slot for members_for<Traits...[trait_index]>[member_index].
struct slot_id {
    std::size_t trait_index;
    std::size_t member_index;
};

consteval std::optional<std::meta::info> field_named(std::meta::info type, std::string_view name) {
    const auto ctx = std::meta::access_context::unprivileged();
    for (const auto member : nonstatic_data_members_of(type, ctx)) {
        if (has_identifier(member) && identifier_of(member) == name) {
            return member;
        }
    }
    return std::nullopt;
}

// Each trait set gets two table types:
// - view_table, whose slots take the object's address. With several traits it
//   holds one view_table per trait; with one mutable trait it holds the table
//   of the trait's const members (const_part) and the mutable slots. Either
//   way, each trait's table is self-contained, so narrowing to one trait or to
//   const is an offset.
// - owning_table, a view_table plus the storage functions duck needs. Narrowing
//   a duck builds a new duck, so its conversions stay pointers.
template <typename... Traits>
struct vtable_generator {
    constexpr static auto ctx = std::meta::access_context::unprivileged();

    constexpr static auto traits = define_static_array(std::vector<std::meta::info>{^^Traits...});

    constexpr static auto can_copy = (std::ranges::any_of(members_for<Traits>, [](auto member) {
        return is_copy_constructor(member);
    }) || ...);
    constexpr static auto is_mutable = (!std::is_const_v<Traits> || ...);

    constexpr static bool is_single = sizeof...(Traits) == 1uz;
    constexpr static bool has_const_part = is_single && is_mutable;
    // Where type_id lives: tables holding sub-tables use their first one's.
    constexpr static bool has_type_id = sizeof...(Traits) == 0uz || (is_single && !is_mutable);

    using const_generator = vtable_generator<const Traits...>;

    struct view_table;
    struct owning_table;

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

    consteval static std::string trait_name(std::size_t index) {
        return "trait_" + index_to_string(index);
    }

    consteval static std::string to_trait_name(std::size_t index) {
        return "to_trait_" + index_to_string(index);
    }

    // A const member of a single mutable trait has its slot in const_part.
    consteval static bool in_const_part(std::meta::info member) {
        return has_const_part && is_const(member);
    }

    consteval static std::size_t const_part_index(std::meta::info member) {
        const auto& const_members = members_for<const Traits...[0]>;
        return static_cast<std::size_t>(std::ranges::distance(
            const_members.begin(), std::ranges::find(const_members, member)));
    }

    consteval {
        std::vector<std::meta::info> members{};
        if constexpr (has_type_id) {
            members.push_back(data_member_spec(^^const type_id_t*, {.name = "type_id"}));
        }
        if constexpr (has_const_part) {
            members.push_back(data_member_spec(^^typename const_generator::view_table,
                {.name = "const_part"}));
        }

        if constexpr (is_single) {
            for (const auto [index, member] : std::views::enumerate(members_for<Traits...[0]>)) {
                if (is_user_provided(member) && !in_const_part(member)) {
                    members.push_back(make_vtable_member(member, index_to_slot_name(0uz, index)));
                }
            }
        } else {
            template for (constexpr auto trait_index : std::views::indices(traits.size())) {
                members.push_back(data_member_spec(
                    ^^typename vtable_generator<Traits...[trait_index]>::view_table,
                    {.name = trait_name(trait_index)}));
            }
            if constexpr (is_mutable) {
                members.push_back(data_member_spec(^^const typename const_generator::view_table*,
                    {.name = "to_const"}));
            }
        }

        define_aggregate(^^view_table, members);
    }

    consteval {
        using allocator = options_data<vtable_generator>::allocator;

        std::vector<std::meta::info> members{
            data_member_spec(^^view_table, {.name = "view"}),
            data_member_spec(^^void(*)(void*, const allocator&) noexcept, {.name = "destroy"}),
            data_member_spec(^^void*(*)(void*, std::byte*), {.name = "fast_move"}),
            data_member_spec(^^void*(*)(void*, std::byte*, const allocator&), {.name = "slow_move"}),
        };
        if constexpr (can_copy) {
            members.push_back(data_member_spec(^^void*(*)(const void*, std::byte*, const allocator&), {.name = "copy"}));
        }
        if constexpr (is_mutable) {
            members.push_back(data_member_spec(^^const typename const_generator::owning_table*,
                {.name = "to_const"}));
        }
        if constexpr (!is_single) {
            template for (constexpr auto trait_index : std::views::indices(traits.size())) {
                members.push_back(data_member_spec(
                    ^^const typename vtable_generator<Traits...[trait_index]>::owning_table*,
                    {.name = to_trait_name(trait_index)}));
            }
        }

        define_aggregate(^^owning_table, members);
    }

    // The function pointer in Slot, wherever it is nested.
    template <slot_id Slot>
    constexpr static auto slot(const view_table& table) noexcept {
        if constexpr (!is_single) {
            constexpr static auto sub = field_named(^^view_table, trait_name(Slot.trait_index)).value();
            return vtable_generator<Traits...[Slot.trait_index]>::template slot<
                slot_id{0uz, Slot.member_index}>(table.[:sub:]);
        } else {
            constexpr static auto member = members_for<Traits...[0]>[Slot.member_index];
            if constexpr (in_const_part(member)) {
                return const_generator::template slot<
                    slot_id{0uz, const_part_index(member)}>(table.const_part);
            } else {
                constexpr static auto field = field_named(^^view_table,
                    index_to_slot_name(0uz, Slot.member_index)).value();
                return table.[:field:];
            }
        }
    }

    constexpr static const type_id_t& type_id(const view_table& table) noexcept {
        if constexpr (has_type_id) {
            return *table.type_id;
        } else if constexpr (has_const_part) {
            return const_generator::type_id(table.const_part);
        } else {
            constexpr static auto sub = field_named(^^view_table, trait_name(0uz)).value();
            return vtable_generator<Traits...[0]>::type_id(table.[:sub:]);
        }
    }

    constexpr static const auto* to_const(const view_table* table) noexcept requires is_mutable {
        if constexpr (has_const_part) {
            return &table->const_part;
        } else {
            return table->to_const;
        }
    }

    constexpr static const auto* to_const(const owning_table* table) noexcept requires is_mutable {
        return table->to_const;
    }

    template <std::size_t Index>
    constexpr static const auto* to_trait(const view_table* table) noexcept requires (!is_single) {
        constexpr static auto sub = field_named(^^view_table, trait_name(Index)).value();
        return &table->[:sub:];
    }

    template <std::size_t Index>
    constexpr static const auto* to_trait(const owning_table* table) noexcept requires (!is_single) {
        constexpr static auto field = field_named(^^owning_table, to_trait_name(Index)).value();
        return table->[:field:];
    }

    // Narrows table to the one trait Trait names, adding const if Trait does.
    template <typename Trait> requires
        ((std::same_as<Traits, Trait> || ...) ||
        (std::same_as<Traits, std::remove_const_t<Trait>> || ...))
    constexpr static const auto* convert(const auto* table) noexcept {
        constexpr static auto trait_itr = std::ranges::find_if(traits, [](auto trait) {
            return trait == ^^Trait || add_const(trait) == ^^Trait;
        });
        constexpr static auto index = static_cast<std::size_t>(std::ranges::distance(traits.begin(), trait_itr));
        constexpr static auto should_constify = is_const(*trait_itr) != is_const(^^Trait);

        const auto* narrowed = [&] {
            if constexpr (is_single) {
                return table;
            } else {
                return to_trait<index>(table);
            }
        }();

        if constexpr (should_constify) {
            return vtable_generator<Traits...[index]>::to_const(narrowed);
        } else {
            return narrowed;
        }
    }

    // The special functions, like move, copy, and destroy, are defined in
    // detail/storage.hpp.
    template <typename T>
    consteval static void set_storage_functions(owning_table& table);

    template <typename T>
    consteval static view_table make_view_table();

    template <typename T>
    consteval static owning_table make_owning_table();

    template <typename T>
    constexpr static auto view_vtable = make_view_table<T>();

    template <typename T>
    constexpr static auto owning_vtable = make_owning_table<T>();

    // Sentinels for an empty duck/duck_view/duck_ptr: type_id is void's and the
    // method slots are null. Conversions lead to the target's null tables,
    // so converting an empty duck yields an empty duck.
    constexpr static auto null_vtable = [] {
        view_table table{};
        if constexpr (has_type_id) {
            table.type_id = &type_id_of<void>();
        }
        if constexpr (has_const_part) {
            table.const_part = const_generator::null_vtable;
        }
        if constexpr (!is_single) {
            template for (constexpr auto trait_index : std::views::indices(traits.size())) {
                constexpr static auto sub = field_named(^^view_table, trait_name(trait_index)).value();
                table.[:sub:] = vtable_generator<Traits...[trait_index]>::null_vtable;
            }
            if constexpr (is_mutable) {
                table.to_const = &const_generator::null_vtable;
            }
        }
        return table;
    }();

    constexpr static auto null_owning_vtable = [] {
        using allocator = options_data<vtable_generator>::allocator;
        owning_table table{};
        table.view = null_vtable;

        if constexpr (is_mutable) {
            table.to_const = &const_generator::null_owning_vtable;
        }
        if constexpr (!is_single) {
            template for (constexpr auto trait_index : std::views::indices(traits.size())) {
                constexpr static auto field = field_named(^^owning_table, to_trait_name(trait_index)).value();
                table.[:field:] = &vtable_generator<Traits...[trait_index]>::null_owning_vtable;
            }
        }

        table.destroy = [](void*, const allocator&) noexcept -> void {};
        table.fast_move = [](void*, std::byte*) noexcept -> void* {
            return nullptr;
        };
        table.slow_move = [](void*, std::byte*, const allocator&) noexcept -> void* {
            return nullptr;
        };
        if constexpr (can_copy) {
            table.copy = [](const void*, std::byte*, const allocator&) noexcept -> void* {
                return nullptr;
            };
        }

        return table;
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

        const auto overload_set_t = choose_set(decay(^^T),
            {.identifier = fixed_string{member_name},
                .param_count = parameters_of(full_sig).size()},
            self_type_for(full_sig, decay(^^T)),
            parameters_of(full_sig));

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
template <typename T>
consteval auto vtable_generator<Traits...>::make_view_table() -> view_table {
    view_table table{};
    if constexpr (has_type_id) {
        table.type_id = &type_id_of<T>();
    }

    // Sub-tables are copied from the tables already made for their traits.
    if constexpr (has_const_part) {
        table.const_part = const_generator::template view_vtable<T>;
    }

    if constexpr (is_single) {
        template for (constexpr auto index : std::views::indices(members_for<Traits...[0]>.size())) {
            constexpr static auto member = members_for<Traits...[0]>[index];
            if constexpr (is_user_provided(member) && !in_const_part(member)
                && !is_copy_constructor(member)) {
                constexpr static auto field = field_named(^^view_table, index_to_slot_name(0uz, index)).value();
                if constexpr (is_operator_function(member)) {
                    constexpr static auto op_maker = get_op_maker<T>(member);
                    table.[:field:] = [:op_maker:]::make();
                } else {
                    constexpr static auto fn_maker = get_fn_maker<T>(traits[0], member);
                    table.[:field:] = [:fn_maker:]::make();
                }
            }
        }
    } else {
        template for (constexpr auto trait_index : std::views::indices(traits.size())) {
            constexpr static auto sub = field_named(^^view_table, trait_name(trait_index)).value();
            table.[:sub:] = vtable_generator<Traits...[trait_index]>::template view_vtable<T>;
        }
        if constexpr (is_mutable) {
            table.to_const = &const_generator::template view_vtable<T>;
        }
    }

    return table;
}

template <typename... Traits>
template <typename T>
consteval auto vtable_generator<Traits...>::make_owning_table() -> owning_table {
    owning_table table{};
    table.view = view_vtable<T>;
    set_storage_functions<T>(table);

    if constexpr (is_mutable) {
        table.to_const = &const_generator::template owning_vtable<T>;
    }
    if constexpr (!is_single) {
        template for (constexpr auto trait_index : std::views::indices(traits.size())) {
            constexpr static auto field = field_named(^^owning_table, to_trait_name(trait_index)).value();
            table.[:field:] = &vtable_generator<Traits...[trait_index]>::template owning_vtable<T>;
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
