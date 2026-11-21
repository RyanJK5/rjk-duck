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

        auto sbo_size_value = default_perf_options{}.sbo_size;
        auto sbo_alignment_value = default_perf_options{}.sbo_alignment;
        auto allocator_value = dealias(^^default_perf_options::allocator);

        // Traits are shown without const, since the const versions of a duck's
        // tables read the same options.
        constexpr static auto handle_conflict =
            [](std::string_view option, std::meta::info prev, std::meta::info trait,
               bool same_value, std::string_view prev_value, std::string_view value) {
                if (!same_value && prev != ^^default_perf_options) {
                    display_error(std::string{option} + " is " + prev_value + " in '" +
                                  display_string_of(remove_const(prev)) + "' but " + value +
                                  " in '" + display_string_of(remove_const(trait)) + "'");
                }
            };

        template for (constexpr auto trait : VtableGenerator::traits) {
            // clang-format off
            using trait_type = typename [:trait:];
            // clang-format on
            if constexpr (requires(trait_type t) { t.sbo_size; }) {
                constexpr auto value = trait_type{}.sbo_size;
                handle_conflict("sbo_size", sbo_size, trait, sbo_size_value == value,
                                index_to_string(sbo_size_value), index_to_string(value));
                sbo_size = trait;
                sbo_size_value = value;
            }
            if constexpr (requires(trait_type t) { t.sbo_alignment; }) {
                constexpr auto value = trait_type{}.sbo_alignment;
                handle_conflict("sbo_alignment", sbo_alignment, trait, sbo_alignment_value == value,
                                index_to_string(sbo_alignment_value), index_to_string(value));
                sbo_alignment = trait;
                sbo_alignment_value = value;
            }
            if constexpr (requires { typename trait_type::allocator; }) {
                constexpr auto value = dealias(^^typename trait_type::allocator);
                handle_conflict("allocator", allocator_type, trait, allocator_value == value,
                                display_string_of(allocator_value), display_string_of(value));
                allocator_type = trait;
                allocator_value = value;
            }
        }

        return std::array{sbo_size, sbo_alignment, allocator_type};
    }

    constexpr static auto options = find_perf_options();

public:
    // clang-format off
    constexpr static auto sbo_size = typename [:options[0UZ]:]{}.sbo_size;

    constexpr static auto sbo_alignment = typename [:options[1UZ]:]{}.sbo_alignment;

    using allocator = typename [:options[2UZ]:]::allocator;
    // clang-format on
};

} // namespace rjk::detail

#endif // RJK_PERF_OPTIONS_HPP