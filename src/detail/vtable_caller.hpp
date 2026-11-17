#ifndef RJK_VTABLE_CALLER_HPP
#define RJK_VTABLE_CALLER_HPP

#include <cassert>
#include <optional>

#include "detail/perf_options.hpp"
#include "detail/flag.hpp"
#include "detail/vtable_generator.hpp"

// Abstraction around vtable to support both regular virtual dispatch and
// inlined function calls.

namespace rjk::detail {

// Holds a duck's (owning_table) or duck_view's (view_table) table, along with
// copies of its [[=rjk::direct]] slots.
template <typename VtableGenerator, typename Table>
class vtable_caller {
private:
    constexpr static auto ctx = std::meta::access_context::unprivileged();

    using allocator = options_data<VtableGenerator>::allocator;

    constexpr static bool owning = std::same_as<Table, typename VtableGenerator::owning_table>;

    struct inlined_functions;

    consteval {
        std::vector<std::meta::info> members{};

        template for (constexpr auto trait_index : std::views::indices(VtableGenerator::traits.size())) {
            const auto& trait_members = members_for<typename [: VtableGenerator::traits[trait_index] :]>;
            for (const auto [index, member] : std::views::enumerate(trait_members)) {
                if (!is_user_provided(member)) {
                    continue;
                }
                if (detail::is_flag_set(member, direct)) {
                    const auto slot = VtableGenerator::make_vtable_member(member,
                        index_to_slot_name(trait_index, index));
                    members.push_back(slot);
                }
            }
        }
        define_aggregate(^^inlined_functions, members);
    }

    constexpr static auto inlined_funcs = define_static_array(
        nonstatic_data_members_of(^^inlined_functions, ctx));

    consteval static std::optional<std::meta::info> inlined_function(slot_id slot) {
        const auto name = index_to_slot_name(slot.trait_index, slot.member_index);
        for (const auto member : inlined_funcs) {
            if (identifier_of(member) == name) {
                return member;
            }
        }
        return std::nullopt;
    }

    constexpr static const auto& view_of(const Table& table) noexcept {
        if constexpr (owning) {
            return table.view;
        } else {
            return table;
        }
    }

    constexpr static const Table* null_table() noexcept {
        if constexpr (owning) {
            return &VtableGenerator::null_owning_vtable;
        } else {
            return &VtableGenerator::null_vtable;
        }
    }

    constexpr static inlined_functions inline_from_vtable([[maybe_unused]] const Table* table) noexcept {
        inlined_functions ret;
        template for (constexpr auto member : inlined_funcs) {
            constexpr static auto indices = extract_indices(identifier_of(member));
            ret.[:member:] = VtableGenerator::template slot<
                slot_id{indices.trait_index, indices.member_index}>(view_of(*table));
        }
        return ret;
    }
public:
    constexpr vtable_caller() noexcept
        : m_inlined(inline_from_vtable(null_table()))
        , m_vtable(null_table())
    { }

    constexpr explicit vtable_caller(const Table* table) noexcept
        : m_inlined(inline_from_vtable(table))
        , m_vtable(table)
    { }

    constexpr void reset() noexcept { m_vtable = null_table(); }
    constexpr const Table* get_vtable() const noexcept { return m_vtable; }

    constexpr const type_id_t& type_id() const noexcept {
        return VtableGenerator::type_id(view_of(*m_vtable));
    }

    // Compares addresses: unlike id_matches, this is safe across shared
    // libraries because void's type_info is defined once, in the C++ runtime.
    constexpr bool has_value() const noexcept { return !id_matches<void>(type_id()); }

    template <typename T>
    constexpr bool has_type() const noexcept { return id_matches<T>(type_id()); }

    template <slot_id Slot, bool Noexcept, typename... Args>
    constexpr decltype(auto) call(auto* underlying, Args&&... args) const noexcept(Noexcept) {
        // The null tables leave the method slots null, so dispatching through
        // one here would call a null function pointer.
        assert(has_value() && "rjk::duck: called member function on valueless duck");
        constexpr static auto inlined = inlined_function(Slot);
        if constexpr (inlined.has_value()) {
            return m_inlined.[:*inlined:](underlying, std::forward<Args>(args)...);
        } else {
            return VtableGenerator::template slot<Slot>(view_of(*m_vtable))(
                underlying, std::forward<Args>(args)...);
        }
    }

    constexpr void destroy(void* obj, const allocator& alloc) const noexcept requires owning {
        m_vtable->destroy(obj, alloc);
    }

    constexpr void* copy(const void* src, std::byte* dest, const allocator& alloc) const requires owning {
        return m_vtable->copy(src, dest, alloc);
    }

    constexpr void* fast_move(void* src, std::byte* dest) const requires owning {
        return m_vtable->fast_move(src, dest);
    }

    constexpr void* slow_move(void* src, std::byte* dest, const allocator& alloc) const requires owning {
        return m_vtable->slow_move(src, dest, alloc);
    }
private:
    [[no_unique_address]] inlined_functions m_inlined;
    const Table* m_vtable;
};

}

#endif // RJK_VTABLE_CALLER_HPP
