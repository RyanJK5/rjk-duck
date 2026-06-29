#ifndef RJK_VTABLE_CALLER_HPP
#define RJK_VTABLE_CALLER_HPP

#include "detail/vtable_generator.hpp"

// Abstraction around vtable to support both regular virtual dispatch and
// inlined function calls.

namespace rjk::detail {

struct default_perf_options {
    std::size_t sbo_size = 32;
    std::size_t sbo_alignment = alignof(std::max_align_t);

    struct inlined_functions {};
};

template <typename VtableGenerator>
class storage;

template <typename VtableGenerator>
class vtable_caller {
private:
    constexpr static auto ctx = std::meta::access_context::unprivileged();

    using options = [: std::invoke([] consteval {
        auto all_traits = template_arguments_of(^^VtableGenerator);

        const auto has_perf_options = [](auto type) {
            return has_annotation(type, ^^perf_options);
        };

        const auto first_itr = std::ranges::find_if(all_traits, has_perf_options);
        if (first_itr == all_traits.end()) {
            return ^^default_perf_options;
        }

        const auto second_itr = std::ranges::find_if(std::next(first_itr),
            all_traits.end(), has_perf_options);
        if (second_itr != all_traits.end()) {
            std::string start{"Found two definitions with [[=rjk::perf_options]]: "};
            display_error(start + display_string_of(*first_itr) + " and "
                + display_string_of(*second_itr));
        }

        return *first_itr;
    }) :];

    consteval static bool options_has_member(std::string_view identifier) {
        return std::ranges::contains(
            members_of(^^options, ctx)
                | std::views::filter(std::meta::has_identifier)
                | std::views::transform(std::meta::identifier_of),
            identifier
        );
    }

    constexpr static auto sbo_size = std::invoke([] {
        if constexpr (options_has_member("sbo_size")) {
            return options{}.sbo_size;
        } else {
            return default_perf_options{}.sbo_size;
        }
    });

    constexpr static auto sbo_alignment = std::invoke([] {
        if constexpr (options_has_member("sbo_alignment")) {
            return options{}.sbo_alignment;
        } else {
            return default_perf_options{}.sbo_alignment;
        }
    });

    struct inlined_functions;

    consteval {
        if constexpr (!options_has_member("inlined_functions")) {
            define_aggregate(^^inlined_functions, {});
        } else {
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
