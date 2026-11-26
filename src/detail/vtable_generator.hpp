#ifndef RJK_VTABLE_GENERATOR_HPP
#define RJK_VTABLE_GENERATOR_HPP

#include <array>
#include <cstddef>
#include <memory>
#include <new>
#include <optional>

#include "detail/vtable_fn_maker.hpp"
#include "duck_tags.hpp"
#include "overload_resolution.hpp"
#include "detail/perf_options.hpp"

#ifdef __cpp_rtti
#include <typeinfo>
#endif

namespace rjk {
struct rtti;
} // namespace rjk

namespace rjk::detail {

consteval bool is_rtti_trait(std::meta::info trait) {
    return is_base_of_type(^^rtti, remove_const(trait));
}

consteval std::string index_to_slot_name(std::integral auto trait_index,
                                         std::integral auto member_index) {
    return "slot_" + index_to_string(trait_index) + "_" + index_to_string(member_index);
}
// slot_0_1

struct index_pair {
    std::size_t trait_index;
    std::size_t member_index;
};

consteval index_pair extract_indices(std::string_view slot) {
    const auto lastUnderscore = slot.find_last_of('_');
    return {.trait_index = string_to_index(slot.substr(5uz, lastUnderscore - 5uz)),
            .member_index = string_to_index(slot.substr(lastUnderscore + 1uz))};
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

// Whether allocator_traits<Alloc>::construct(alloc, obj, args...) can't throw.
// That function isn't declared noexcept, so this asks what it calls: Alloc's
// construct if it has one, otherwise T's constructor.
template <typename Alloc, typename T, typename... Args>
constexpr bool is_nothrow_allocator_constructible = [] consteval {
    if constexpr (requires(Alloc& alloc, T* obj, Args&&... args) {
                      alloc.construct(obj, std::forward<Args>(args)...);
                  }) {
        return noexcept(
            std::declval<Alloc&>().construct(std::declval<T*>(), std::declval<Args>()...));
    } else {
        return std::is_nothrow_constructible_v<T, Args...>;
    }
}();

// Whether moving a T into new storage through Alloc can't throw. Only asks the
// allocator once T is known to be movable, since its construct may name T's
// move constructor in its exception specification.
template <typename Alloc, typename T>
constexpr bool is_nothrow_allocator_move_constructible = [] consteval {
    if constexpr (std::is_move_constructible_v<T>) {
        return is_nothrow_allocator_constructible<Alloc, T, T&&>;
    } else {
        return false;
    }
}();

// Where a duck's object is stored.
enum struct storage_kind { none, heap, buffered };

// A duck's object: in buffer if it fits, otherwise on the heap through ptr.
template <std::size_t Size, std::size_t Align>
union small_buffer_storage {
    void* ptr = nullptr;

    alignas(Align) std::array<std::byte, Size> buffer;

    // Makes buffer the active member, so an object can be created in it.
    std::byte* start_buffer() noexcept {
        ::new (static_cast<void*>(std::addressof(buffer))) std::array<std::byte, Size>;
        return buffer.data();
    }
};

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

    constexpr static auto can_copy =
        (std::ranges::any_of(members_for<Traits>,
                             [](auto member) { return is_copy_constructor(member); }) ||
         ...);
    constexpr static auto is_mutable = (!std::is_const_v<Traits> || ...);

    constexpr static bool is_single = sizeof...(Traits) == 1uz;
    constexpr static bool has_const_part = is_single && is_mutable;
    constexpr static bool has_rtti = (is_rtti_trait(^^Traits) || ...);
#ifndef __cpp_rtti
    static_assert(!has_rtti, "rjk::rtti requires RTTI");
#endif
    // Only an rjk::rtti trait's table stores type_id, so the other traits'
    // sub-tables look the same as they would without it.
    constexpr static bool stores_identity = is_single && !is_mutable && has_rtti;

    using const_generator = vtable_generator<const Traits...>;

    struct view_table;
    struct owning_table;

    // A slot for member, whose first parameter points to erased: the object
    // (void) for view tables, or the storage for owning tables.
    consteval static std::meta::info make_vtable_member(std::meta::info member,
                                                        std::string_view name,
                                                        std::meta::info erased = ^^void) {
        const auto erased_ptr_type = add_pointer(is_const(member) ? add_const(erased) : erased);
        const auto sig = remove_fn_qualifiers(type_of(member));
        const auto ptr_type = add_pointer(prepend_arg(erased_ptr_type, sig));
        return data_member_spec(ptr_type, {.name = name});
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
        return static_cast<std::size_t>(
            std::ranges::distance(const_members.begin(), std::ranges::find(const_members, member)));
    }

    consteval {
        std::vector<std::meta::info> members{};
#ifdef __cpp_rtti
        if constexpr (stores_identity) {
            members.push_back(data_member_spec(^^const std::type_info*, {.name = "type_id"}));
        }
#endif
        if constexpr (has_const_part) {
            members.push_back(
                data_member_spec(^^typename const_generator::view_table, {.name = "const_part"}));
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
                    ^^typename vtable_generator<Traits... [trait_index]>::view_table,
                    { .name = trait_name(trait_index) }));
            }
            if constexpr (is_mutable) {
                members.push_back(data_member_spec(^^const typename const_generator::view_table*,
                                                   {.name = "to_const"}));
            }
        }

        define_aggregate(^^view_table, members);
    }

    using storage_type = small_buffer_storage<options_data<vtable_generator>::sbo_size,
                                              options_data<vtable_generator>::sbo_alignment>;

    template <typename T>
    using rebound_allocator = std::allocator_traits<
        typename options_data<vtable_generator>::allocator>::template rebind_alloc<T>;

    // Whether a T goes in storage_type's buffer: it has to fit, and relocating
    // it through the allocator can't throw, so moving a duck stays noexcept.
    template <typename T>
    constexpr static bool fits_buffer =
        sizeof(T) <= options_data<vtable_generator>::sbo_size &&
        alignof(T) <= options_data<vtable_generator>::sbo_alignment &&
        is_nothrow_allocator_move_constructible<rebound_allocator<T>, T>;

    // The storage functions put an object into an empty storage_type. Those that
    // take the source object by address work across ducks whose storage differs.
    consteval {
        using allocator = options_data<vtable_generator>::allocator;

        std::vector<std::meta::info> members{
            data_member_spec(^^view_table, {.name = "view"}),
            data_member_spec(^^storage_kind, {.name = "kind"}),
            // Destroys the object, leaving the storage empty.
            data_member_spec(^^void (*)(storage_type&, const allocator&) noexcept,
                             {.name = "destroy"}),
            // Moves the object to another storage of the same duck, leaving the
            // source empty. The allocator must equal the one that built it.
            data_member_spec(^^void (*)(storage_type&, storage_type&, const allocator&) noexcept,
                             {.name = "relocate"}),
            // Move-constructs the object at the address into the storage, using the allocator.
            data_member_spec(^^void (*)(void*, storage_type&, const allocator&), {.name = "move"}),
        };
        if constexpr (can_copy) {
            members.push_back(data_member_spec(
                ^^void (*)(const void*, storage_type&, const allocator&), {.name = "copy"}));
        }
        if constexpr (is_mutable) {
            members.push_back(data_member_spec(^^const typename const_generator::owning_table*,
                                               {.name = "to_const"}));
        }
        if constexpr (!is_single) {
            template for (constexpr auto trait_index : std::views::indices(traits.size())) {
                members.push_back(data_member_spec(
                    ^^const typename vtable_generator<Traits... [trait_index]>::owning_table*,
                    { .name = to_trait_name(trait_index) }));
            }
        }

        // A duck calls through these rather than the view slots, so a call
        // doesn't have to look up where the object is.
        template for (constexpr auto trait_index : std::views::indices(traits.size())) {
            for (const auto [index, member] :
                 std::views::enumerate(members_for<Traits...[trait_index]>)) {
                if (is_user_provided(member)) {
                    members.push_back(make_vtable_member(
                        member, index_to_slot_name(trait_index, index), ^^storage_type));
                }
            }
        }

        define_aggregate(^^owning_table, members);
    }

    // The owning table's function pointer for Slot.
    template <slot_id Slot>
    constexpr static auto owning_slot(const owning_table& table) noexcept {
        constexpr static auto field =
            field_named(^^owning_table, index_to_slot_name(Slot.trait_index, Slot.member_index))
                .value();
        return table.[:field:];
    }

    // The function pointer in Slot, wherever it is nested.
    template <slot_id Slot>
    constexpr static auto slot(const view_table& table) noexcept {
        if constexpr (!is_single) {
            constexpr static auto sub =
                field_named(^^view_table, trait_name(Slot.trait_index)).value();
            return vtable_generator<Traits... [Slot.trait_index]>::template slot<slot_id {
                0uz, Slot.member_index
            }>(table.[:sub:]);
        } else {
            constexpr static auto member = members_for<Traits...[0]>[Slot.member_index];
            if constexpr (in_const_part(member)) {
                return const_generator::template slot<slot_id{0uz, const_part_index(member)}>(
                    table.const_part);
            } else {
                constexpr static auto field =
                    field_named(^^view_table, index_to_slot_name(0uz, Slot.member_index)).value();
                return table.[:field:];
            }
        }
    }

#ifdef __cpp_rtti
    // The sub-table that stores type_id.
    constexpr static const auto& identity_table(const view_table& table) noexcept
        requires has_rtti
    {
        if constexpr (stores_identity) {
            return table;
        } else if constexpr (has_const_part) {
            return const_generator::identity_table(table.const_part);
        } else {
            constexpr static auto index = static_cast<std::size_t>(
                std::ranges::find_if(traits, is_rtti_trait) - traits.begin());
            constexpr static auto sub = field_named(^^view_table, trait_name(index)).value();
            return vtable_generator<Traits...[index]>::identity_table(table.[:sub:]);
        }
    }

    constexpr static const std::type_info& type_id(const view_table& table) noexcept
        requires has_rtti
    {
        return *identity_table(table).type_id;
    }
#endif

    constexpr static const auto* to_const(const view_table* table) noexcept
        requires is_mutable
    {
        if constexpr (has_const_part) {
            return &table->const_part;
        } else {
            return table->to_const;
        }
    }

    constexpr static const auto* to_const(const owning_table* table) noexcept
        requires is_mutable
    {
        return table->to_const;
    }

    template <std::size_t Index>
    constexpr static const auto* to_trait(const view_table* table) noexcept
        requires(!is_single)
    {
        constexpr static auto sub = field_named(^^view_table, trait_name(Index)).value();
        return &table->[:sub:];
    }

    template <std::size_t Index>
    constexpr static const auto* to_trait(const owning_table* table) noexcept
        requires(!is_single)
    {
        constexpr static auto field = field_named(^^owning_table, to_trait_name(Index)).value();
        return table->[:field:];
    }

    // Narrows table to the one trait Trait names, adding const if Trait does.
    template <typename Trait>
        requires((std::same_as<Traits, Trait> || ...) ||
                 (std::same_as<Traits, std::remove_const_t<Trait>> || ...))
    constexpr static const auto* convert(const auto* table) noexcept {
        constexpr static auto trait_itr = std::ranges::find_if(
            traits, [](auto trait) { return trait == ^^Trait || add_const(trait) == ^^Trait; });
        constexpr static auto index =
            static_cast<std::size_t>(std::ranges::distance(traits.begin(), trait_itr));
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

    // The storage functions, like move, copy, and destroy, are defined in
    // detail/erased_object.hpp.
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

    // Sentinels for an empty duck/duck_view/duck_ptr: type_id is void's (with
    // rjk::rtti) and the method slots are null. Conversions lead to the target's null tables,
    // so converting an empty duck yields an empty duck.
    constexpr static auto null_vtable = [] {
        view_table table{};
#ifdef __cpp_rtti
        if constexpr (stores_identity) {
            table.type_id = &typeid(void);
        }
#endif
        if constexpr (has_const_part) {
            table.const_part = const_generator::null_vtable;
        }
        if constexpr (!is_single) {
            template for (constexpr auto trait_index : std::views::indices(traits.size())) {
                constexpr static auto sub =
                    field_named(^^view_table, trait_name(trait_index)).value();
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
                constexpr static auto field =
                    field_named(^^owning_table, to_trait_name(trait_index)).value();
                table.[:field:] = &vtable_generator<Traits...[trait_index]>::null_owning_vtable;
            }
        }

        table.kind = storage_kind::none;

        table.destroy = [](storage_type&, const allocator&) noexcept {};
        table.relocate = [](storage_type&, storage_type& dest, const allocator&) noexcept {
            dest.ptr = nullptr;
        };
        table.move = [](void*, storage_type& dest, const allocator&) noexcept {
            dest.ptr = nullptr;
        };
        if constexpr (can_copy) {
            table.copy = [](const void*, storage_type& dest, const allocator&) noexcept {
                dest.ptr = nullptr;
            };
        }

        return table;
    }();

    // What a slot for trait's member calls on a T: its impl, or the member
    // overload resolution picks.
    template <typename T>
    consteval static std::meta::info invoker_for(std::meta::info trait, std::meta::info member) {
        const auto full_sig = type_of(member);
        const auto member_name = identifier_of(member);

        const auto impl = find_impl_specialization(^^T, trait, member_name, full_sig);
        if (impl.has_value()) {
            return impl.value();
        }

        return choose_set(decay(^^T),
                          {.identifier = fixed_string{member_name},
                           .param_count = parameters_of(full_sig).size()},
                          self_type_for(full_sig, decay(^^T)), parameters_of(full_sig));
    }

    // invoker_for a single trait's member, resolved once and shared by T's view
    // and owning tables.
    template <typename T, std::size_t MemberIndex>
    constexpr static std::meta::info invoker =
        invoker_for<T>(traits[0], members_for<Traits...[0]>[MemberIndex]);

    // The invoker behind Slot, from the table the view slot is resolved in.
    template <typename T, slot_id Slot>
    consteval static std::meta::info invoker_at() {
        if constexpr (!is_single) {
            return vtable_generator<Traits... [Slot.trait_index]>::template invoker_at<T, slot_id {
                0uz, Slot.member_index
            }>();
        } else if constexpr (in_const_part(members_for<Traits...[0]>[Slot.member_index])) {
            return const_generator::template invoker_at<
                T, slot_id{0uz, const_part_index(members_for<Traits...[0]>[Slot.member_index])}>();
        } else {
            return invoker<T, Slot.member_index>;
        }
    }

    template <typename T, slot_id Slot, typename Access>
    consteval static std::meta::info fn_maker_for() {
        const auto member = members_for<Traits...[Slot.trait_index]>[Slot.member_index];
        return substitute(^^vtable_fn_maker, {remove_fn_qualifiers(type_of(member)),
                                              std::meta::reflect_constant(qualifiers_of(member)),
                                              ^^T, invoker_at<T, Slot>(), ^^Access});
    }

    template <typename T, typename Access>
    consteval static std::meta::info op_maker_for(std::meta::info member) {
        return substitute(^^vtable_op_maker,
                          {remove_fn_qualifiers(type_of(member)),
                           std::meta::reflect_constant(qualifiers_of(member)),
                           reflect_constant(operator_of(member)),
                           std::meta::reflect_constant(op_kind_of(member)), ^^T, ^^Access});
    }
};

template <typename... Traits>
template <typename T>
consteval auto vtable_generator<Traits...>::make_view_table() -> view_table {
    view_table table{};
#ifdef __cpp_rtti
    if constexpr (stores_identity) {
        table.type_id = &typeid(T);
    }
#endif

    // Sub-tables are copied from the tables already made for their traits.
    if constexpr (has_const_part) {
        table.const_part = const_generator::template view_vtable<T>;
    }

    if constexpr (is_single) {
        template for (constexpr auto index :
                      std::views::indices(members_for<Traits...[0]>.size())) {
            constexpr static auto member = members_for<Traits...[0]>[index];
            if constexpr (is_user_provided(member) && !in_const_part(member) &&
                          !is_copy_constructor(member)) {
                constexpr static auto field =
                    field_named(^^view_table, index_to_slot_name(0uz, index)).value();
                if constexpr (is_operator_function(member)) {
                    constexpr static auto op_maker = op_maker_for<T, address_access>(member);
                    table.[:field:] = [:op_maker:] ::make();
                } else {
                    constexpr static auto fn_maker =
                        fn_maker_for<T, slot_id{0uz, index}, address_access>();
                    table.[:field:] = [:fn_maker:] ::make();
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
            constexpr static auto field =
                field_named(^^owning_table, to_trait_name(trait_index)).value();
            table.[:field:] = &vtable_generator<Traits...[trait_index]>::template owning_vtable<T>;
        }
    }

    using access = storage_access<storage_type, fits_buffer<T>>;
    template for (constexpr auto trait_index : std::views::indices(traits.size())) {
        template for (constexpr auto index :
                      std::views::indices(members_for<Traits...[trait_index]>.size())) {
            constexpr static auto member = members_for<Traits...[trait_index]>[index];
            if constexpr (is_user_provided(member) && !is_copy_constructor(member)) {
                constexpr static auto field =
                    field_named(^^owning_table, index_to_slot_name(trait_index, index)).value();
                if constexpr (is_operator_function(member)) {
                    constexpr static auto op_maker = op_maker_for<T, access>(member);
                    table.[:field:] = [:op_maker:] ::make();
                } else {
                    constexpr static auto fn_maker =
                        fn_maker_for<T, slot_id{trait_index, index}, access>();
                    table.[:field:] = [:fn_maker:] ::make();
                }
            }
        }
    }

    return table;
}

consteval std::meta::info make_vtable_generator(std::vector<std::meta::info> traits) {
    std::ranges::sort(traits, [](auto a, auto b) { return std::is_lt(compare_meta_info(a, b)); });
    return substitute(^^vtable_generator, traits);
}

consteval std::meta::info make_vtable_generator(std::meta::info duck_type) {
    return make_vtable_generator(template_arguments_of(decay(duck_type)));
}

} // namespace rjk::detail

#endif
