#ifndef RJK_PERF_OPTIONS_HPP
#define RJK_PERF_OPTIONS_HPP

#include "detail/vtable_generator.hpp"

namespace rjk::detail {

struct default_perf_options {
    std::size_t sbo_size = sizeof(void*) * 2UZ;
    std::size_t sbo_alignment = alignof(std::max_align_t);

    using allocator = std::allocator<std::byte>;
};

template <typename VtableGenerator>
struct options_data {
    constexpr static auto ctx = std::meta::access_context::unprivileged();

    using type = [: std::invoke([] consteval {
        auto all_traits = template_arguments_of(^^VtableGenerator);

        const auto has_perf_options = [](auto type) {
            return detail::is_flag_set(type, perf_options);
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
            members_of(^^type, ctx)
                | std::views::filter(std::meta::has_identifier)
                | std::views::transform(std::meta::identifier_of),
            identifier
        );
    }
public:
    using allocator = [: std::invoke([] {
        if constexpr (options_has_member("allocator")) {
            return ^^typename type::allocator;
        } else {
            return ^^typename default_perf_options::allocator;
        }
    }) :];

    constexpr static auto sbo_size = std::invoke([] {
        if constexpr (options_has_member("sbo_size")) {
            return type{}.sbo_size;
        } else {
            return default_perf_options{}.sbo_size;
        }
    });

    constexpr static auto sbo_alignment = std::invoke([] {
        if constexpr (options_has_member("sbo_alignment")) {
            return type{}.sbo_alignment;
        } else {
            return default_perf_options{}.sbo_alignment;
        }
    });
};

}

#endif // RJK_PERF_OPTIONS_HPP