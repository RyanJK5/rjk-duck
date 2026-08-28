#ifndef RJK_VTABLE_CALLER_HPP
#define RJK_VTABLE_CALLER_HPP

#include "detail/perf_options.hpp"
#include "detail/flag.hpp"

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

    constexpr static auto vtable_funcs = define_static_array(
        nonstatic_data_members_of(^^vtable, ctx));

    consteval static bool is_inlined_function(std::meta::info member) {
        return !std::ranges::contains(vtable_funcs, member);
    }
public:
    friend storage<VtableGenerator>;

    consteval static std::meta::info get_callable(std::integral auto trait_index,
        std::integral auto member_index) {
        const auto matching_index = [=](auto member) {
            return identifier_of(member) == index_to_slot_name(trait_index, member_index);
        };

        const auto inlined_funcs = nonstatic_data_members_of(^^inlined_functions, ctx);
        if (const auto itr = std::ranges::find_if(inlined_funcs, matching_index);
            itr != inlined_funcs.end()) {
            return *itr;
        }

        return *std::ranges::find_if(vtable_funcs, matching_index);
    }

    constexpr static inlined_functions inline_from_vtable(const vtable* v) noexcept {
        inlined_functions ret;
        template for (constexpr auto member : define_static_array(
            nonstatic_data_members_of(^^inlined_functions, ctx))) {
            constexpr static auto itr = std::ranges::find_if(vtable_funcs, [](auto v_member) {
                return identifier_of(member) == identifier_of(v_member);
            });
            if constexpr (itr != vtable_funcs.end()) {
                ret.[:member:] = v->[:(*itr):];
            }
        }
        return ret;
    }
public:
    constexpr vtable_caller() noexcept
        : m_vtable(&VtableGenerator::null_vtable)
    { }

    constexpr explicit vtable_caller(const vtable* v) noexcept
        : m_inlined(inline_from_vtable(v))
        , m_vtable(v)
    { }

    constexpr void reset() noexcept { m_vtable = &VtableGenerator::null_vtable; }
    constexpr const auto* get_vtable() const noexcept { return m_vtable; }

    constexpr bool has_value() const noexcept { return m_vtable != &VtableGenerator::null_vtable; }

    template <std::meta::info Member, bool Noexcept, typename... Args>
    constexpr decltype(auto) call(auto* underlying, Args&&... args) const noexcept(Noexcept) {
        if constexpr (is_inlined_function(Member)) {
            return m_inlined.[:Member:](underlying, std::forward<Args>(args)...);
        } else {
            return m_vtable->[:Member:](underlying, std::forward<Args>(args)...);
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
