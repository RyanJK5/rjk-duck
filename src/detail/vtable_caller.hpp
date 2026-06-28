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

        const auto member_identifiers = [](std::meta::info class_type) {
            const auto ctx = std::meta::access_context::unprivileged();
            return members_of(class_type, ctx)
                | std::views::filter(std::meta::has_identifier)
                | std::views::transform(std::meta::identifier_of)
                | std::ranges::to<std::vector>();
        };
        const auto fields = member_identifiers(*first_itr);
        for (const auto identifier : member_identifiers(^^default_perf_options)) {
            const bool has_member = std::ranges::contains(fields, identifier);
            if (!has_member) {
                display_error(std::string{"customized performance options '"}
                    + identifier_of(*first_itr) +"' are missing field '"
                    + identifier + "'");
            }
        }
        return *first_itr;
    }) :];

    constexpr static auto sbo_size = options{}.sbo_size;
    constexpr static auto sbo_alignment = options{}.sbo_alignment;

    struct inlined_functions;

    consteval {
        const auto tags = members_to_tags(^^typename options::inlined_functions);
        std::vector<std::meta::info> members{};
        std::size_t index{};
        for (const auto tag : tags) {
            members.push_back(VtableGenerator::make_vtable_member(
            tag, index_to_slot_name(index)));
            index++;
        }

        define_aggregate(^^inlined_functions, members);
    }
public:
    friend storage<VtableGenerator>;

    consteval static std::meta::info get_callable(std::size_t tag_index) {
        const auto members = nonstatic_data_members_of(
            ^^typename VtableGenerator::vtable,
            std::meta::access_context::unprivileged()
        );

        return *std::ranges::find_if(members,
            [tag_index](auto member) { return identifier_of(member) == index_to_slot_name(tag_index); }
        );
    }
public:
    
private:
    const typename VtableGenerator::vtable* m_vtable;
    [[no_unique_address]] inlined_functions m_inlined;
};

}

#endif // RJK_VTABLE_CALLER_HPP
