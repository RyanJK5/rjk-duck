#ifndef RJK_PERF_OPTIONS_HPP
#define RJK_PERF_OPTIONS_HPP

#include "detail/vtable_generator.hpp"

namespace rjk::detail {

struct default_perf_options {
    std::size_t sbo_size = sizeof(void*) * 3UZ;
    std::size_t sbo_alignment = alignof(void*);

    using allocator = std::allocator<std::byte>;
};

// Whether type or one of its bases declares a data member or type named name.
// Member functions are a trait's interface, so one named like an option isn't one.
consteval bool declares_option(std::meta::info type, std::string_view name) {
    constexpr auto ctx = std::meta::access_context::unchecked();
    for (const auto member : members_of(type, ctx)) {
        if (!is_function(member) && !is_function_template(member) && has_identifier(member) &&
            identifier_of(member) == name) {
            return true;
        }
    }
    for (const auto base : bases_of(type, ctx)) {
        if (declares_option(type_of(base), name)) {
            return true;
        }
    }
    return false;
}

// The performance options a duck's traits set. Several traits may set the same
// option, but only to the same value.
template <typename VtableGenerator>
struct options_data {
private:
    struct found_options {
        std::size_t sbo_size = default_perf_options{}.sbo_size;
        std::size_t sbo_alignment = default_perf_options{}.sbo_alignment;
        std::meta::info allocator = ^^typename default_perf_options::allocator;

        // The trait each option came from, or null for the default.
        std::meta::info sbo_size_source{};
        std::meta::info sbo_alignment_source{};
        std::meta::info allocator_source{};
    };

    consteval static void merge(std::string_view option, std::meta::info& source,
                                std::meta::info trait, bool same_value, std::string_view old_value,
                                std::string_view new_value) {
        if (source != std::meta::info{} && !same_value) {
            display_error(std::string{option} + " is " + std::string{old_value} + " in '" +
                          std::string{display_string_of(source)} + "' but " +
                          std::string{new_value} + " in '" + std::string{display_string_of(trait)} +
                          "'");
        }
        source = trait;
    }

    // An option the trait declares but can't be read from, e.g. one that's
    // private or inherited from two bases, would otherwise be ignored.
    consteval static void check_readable(std::meta::info trait, std::string_view option) {
        if (declares_option(trait, option)) {
            display_error(std::string{"'"} + std::string{display_string_of(trait)} + "' declares " +
                          std::string{option} + ", but it isn't a public, unambiguous member");
        }
    }

    consteval static found_options find() {
        found_options found{};

        template for (constexpr auto trait : VtableGenerator::traits) {
            // Report traits as written: the const versions of a duck's tables
            // read the same options.
            constexpr auto shown = remove_const(trait);
            using trait_type = typename[:shown:];

            if constexpr (requires(trait_type t) { t.sbo_size; }) {
                constexpr std::size_t value = trait_type{}.sbo_size;
                merge("sbo_size", found.sbo_size_source, shown, found.sbo_size == value,
                      index_to_string(found.sbo_size), index_to_string(value));
                found.sbo_size = value;
            } else {
                check_readable(shown, "sbo_size");
            }

            if constexpr (requires(trait_type t) { t.sbo_alignment; }) {
                constexpr std::size_t value = trait_type{}.sbo_alignment;
                merge("sbo_alignment", found.sbo_alignment_source, shown,
                      found.sbo_alignment == value, index_to_string(found.sbo_alignment),
                      index_to_string(value));
                found.sbo_alignment = value;
            } else {
                check_readable(shown, "sbo_alignment");
            }

            if constexpr (requires { typename trait_type::allocator; }) {
                constexpr auto value = dealias(^^typename trait_type::allocator);
                merge("allocator", found.allocator_source, shown, found.allocator == value,
                      display_string_of(found.allocator), display_string_of(value));
                found.allocator = value;
            } else {
                check_readable(shown, "allocator");
            }
        }

        return found;
    }

    constexpr static auto options = find();

public:
    constexpr static std::size_t sbo_size = options.sbo_size;
    constexpr static std::size_t sbo_alignment = options.sbo_alignment;
    using allocator = [:options.allocator:];
};

} // namespace rjk::detail

#endif // RJK_PERF_OPTIONS_HPP