#ifndef RJK_VTABLE_CALLER_HPP
#define RJK_VTABLE_CALLER_HPP

#include <cassert>

#include "detail/perf_options.hpp"
#include "detail/flag.hpp"
#include "detail/vtable_generator.hpp"

// Abstraction around vtable to support both regular virtual dispatch and
// inlined function calls.

namespace rjk::detail {

template <typename VtableGenerator>
class storage;

template <typename VtableGenerator>
class vtable_caller {
private:
    constexpr static auto ctx = std::meta::access_context::unprivileged();

    using options = options_data<VtableGenerator>;
    using allocator = storage<VtableGenerator>::allocator_type;

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

    using vtable = VtableGenerator::vtable;

    constexpr static auto inlined_funcs = define_static_array(
        nonstatic_data_members_of(^^inlined_functions, ctx));

    consteval static std::meta::info inlined_function(slot_id slot) {
        const auto name = index_to_slot_name(slot.trait_index, slot.member_index);
        for (const auto member : inlined_funcs) {
            if (identifier_of(member) == name) {
                return member;
            }
        }
        return {};
    }
public:
    friend storage<VtableGenerator>;

    constexpr static inlined_functions inline_from_vtable([[maybe_unused]] const vtable* v) noexcept {
        inlined_functions ret;
        template for (constexpr auto member : inlined_funcs) {
            constexpr static auto indices = extract_indices(identifier_of(member));
            ret.[:member:] = VtableGenerator::template slot<
                slot_id{indices.trait_index, indices.member_index}>(*v);
        }
        return ret;
    }
public:
    constexpr vtable_caller() noexcept
        : m_inlined(inline_from_vtable(&VtableGenerator::null_vtable))
        , m_vtable(&VtableGenerator::null_vtable)
    { }

    constexpr explicit vtable_caller(const vtable* v) noexcept
        : m_inlined(inline_from_vtable(v))
        , m_vtable(v)
    { }

    constexpr void reset() noexcept { m_vtable = &VtableGenerator::null_vtable; }
    constexpr const auto* get_vtable() const noexcept { return m_vtable; }

    constexpr bool has_value() const noexcept { return m_vtable != &VtableGenerator::null_vtable; }

    template <slot_id Slot, bool Noexcept, typename... Args>
    constexpr decltype(auto) call(auto* underlying, Args&&... args) const noexcept(Noexcept) {
        // null_vtable fills in the storage functions but leaves the method slots
        // null, so dispatching through it here would call a null function pointer.
        assert(has_value() && "rjk::duck: called member function on valueless duck");
        constexpr static auto inlined = inlined_function(Slot);
        if constexpr (inlined != std::meta::info{}) {
            return m_inlined.[:inlined:](underlying, std::forward<Args>(args)...);
        } else {
            return VtableGenerator::template slot<Slot>(*m_vtable)(underlying, std::forward<Args>(args)...);
        }
    }

    constexpr void destroy(void* obj, const allocator& alloc) const noexcept {
        m_vtable->destroy(obj, alloc);
    }

    constexpr void* copy(const void* src, std::byte* dest, const allocator& alloc) const {
        return m_vtable->copy(src, dest, alloc);
    }

    constexpr void* fast_move(void* src, std::byte* dest) const {
        return m_vtable->fast_move(src, dest);
    }

    constexpr void* slow_move(void* src, std::byte* dest, const allocator& alloc) const {
        return m_vtable->slow_move(src, dest, alloc);
    }
private:
    [[no_unique_address]] inlined_functions m_inlined;
    const vtable* m_vtable;
};

}

#endif // RJK_VTABLE_CALLER_HPP
