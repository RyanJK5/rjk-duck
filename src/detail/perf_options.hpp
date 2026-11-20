#ifndef RJK_PERF_OPTIONS_HPP
#define RJK_PERF_OPTIONS_HPP

#include "detail/vtable_generator.hpp"

namespace rjk::detail {

struct default_perf_options {
    std::size_t sbo_size = sizeof(void*) * 3UZ;
    std::size_t sbo_alignment = alignof(void*);

    using allocator = std::allocator<std::byte>;
};

template <typename VtableGenerator>
struct options_data {
    constexpr static auto ctx = std::meta::access_context::unprivileged();

    consteval static auto find_perf_options() {
        auto sbo_size = ^^default_perf_options;
        auto sbo_alignment = ^^default_perf_options;
        auto allocator_type = ^^default_perf_options;

        constexpr static auto handle_conflict = [](std::meta::info prev, std::meta::info trait) {
            if (prev != ^^default_perf_options) {
                display_error(std::string{"sbo_size is defined by both " }
                    + display_string_of(prev)
                    + " and " 
                    + display_string_of(trait));
            }
        };

        template for (constexpr auto trait : VtableGenerator::traits) {
            using trait_type = typename [:trait:];
            if constexpr (requires(trait_type t) { t.sbo_size; }) {
                handle_conflict(sbo_size, trait);
                sbo_size = trait;
            }
            if constexpr (requires(trait_type t) { t.sbo_alignment; }) {
                handle_conflict(sbo_alignment, trait);
                sbo_alignment = trait;
            }
            if constexpr (requires { typename trait_type::allocator; }) {
                handle_conflict(allocator_type, trait);
                allocator_type = trait;
            }
        }

        return std::array{sbo_size, sbo_alignment, allocator_type};
    }

    constexpr static auto options = find_perf_options();
public:
    constexpr static auto sbo_size =
        typename [: options[0UZ] :]{}.sbo_size;

    constexpr static auto sbo_alignment =
        typename [: options[1UZ] :]{}.sbo_alignment;

    using allocator = 
        typename [: options[2UZ] :]::allocator;
};

} // namespace rjk::detail

#endif // RJK_PERF_OPTIONS_HPP