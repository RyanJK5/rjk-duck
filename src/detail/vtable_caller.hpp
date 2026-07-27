#ifndef RJK_VTABLE_CALLER_HPP
#define RJK_VTABLE_CALLER_HPP

#include "detail/perf_options.hpp"

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

    struct inlined_functions;

    consteval {
        const auto tags = members_to_tags(^^typename options::inlined_functions);

        const auto members = VtableGenerator::tags
            | std::views::enumerate
            | std::views::filter([&tags](auto pair) {
                const auto [_, tag] = pair;
                return std::ranges::contains(tags, tag);
            })
            | std::views::transform([](auto pair) {
                const auto [index, tag] = pair;
                return VtableGenerator::make_vtable_member(tag, index_to_slot_name(index));
            })
            | std::ranges::to<std::vector>();
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

    consteval static std::meta::info get_callable(std::size_t tag_index) {
        const auto matching_index = [tag_index](auto member) {
            return identifier_of(member) == index_to_slot_name(tag_index);
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
    constexpr explicit vtable_caller(const vtable* v) noexcept
        : m_inlined(inline_from_vtable(v))
        , m_vtable(v)
    { }

    constexpr const auto* get_vtable() const noexcept { return m_vtable; }

    constexpr void reset() noexcept { m_vtable = nullptr; }

    template <std::meta::info Member, bool Noexcept, typename... Args>
    constexpr decltype(auto) call(auto* underlying, Args&&... args) const noexcept(Noexcept) {
        if constexpr (is_inlined_function(Member)) {
            return m_inlined.[:Member:](underlying, std::forward<Args>(args)...);
        } else {
            return m_vtable->[:Member:](underlying, std::forward<Args>(args)...);
        }
    }
private:
    [[no_unique_address]] inlined_functions m_inlined;
    const vtable* m_vtable;
};

}

#endif // RJK_VTABLE_CALLER_HPP
