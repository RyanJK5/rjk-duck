// clang-format off

#ifndef RJK_SUBSUMPTION_UTILS_HPP
#define RJK_SUBSUMPTION_UTILS_HPP

#include <meta>

#include "detail/vtable_generator.hpp"

namespace rjk::detail {

consteval bool is_duck_view(std::meta::info type) {
    return has_template_arguments(type)
        && is_type(type)
        && template_of(type) == ^^duck_view;
}

// Provides some helper functions for determining if a duck type subsumes another
// duck type.
template <is_trait... Traits>
struct subsumption_utils {
    constexpr static std::array<std::meta::info, sizeof...(Traits)>
        traits{^^Traits...};

    consteval static bool total_subsumption(std::meta::info type) {
        if (!is_duck_type(type)) {
            return false;
        }

        const auto args = template_arguments_of(type);
        if (sizeof...(Traits) == 1 && args.size() != 1) {
            return false;
        }
        return std::ranges::equal(traits, args);
    }

    consteval static bool total_const_subsumption(std::meta::info type) {
        if (sizeof...(Traits) == 1) {
            return false;
        }
        if (!is_duck_type(type)) {
            return false;
        }

        const auto args = template_arguments_of(type);

        if (std::ranges::all_of(args, std::meta::is_const)) {
            return false;
        }

        return std::ranges::equal(traits,
            args | std::views::transform(std::meta::add_const)
        );
    }

    consteval static bool single_trait_subsumption(std::meta::info type) {
        if (!is_duck_type(type)) {
            return false;
        }
        if (total_subsumption(type)) {
            return false;
        }
        if (sizeof...(Traits) != 1) {
            return false;
        }

        const auto args = template_arguments_of(type);
        return std::ranges::contains(
            args | std::views::transform(std::meta::remove_const),
            remove_const(*traits.begin())
        );
    }

    template <typename Duck>
    constexpr static const vtable_generator<Traits...>::vtable* convert_from(const auto* table) {
        constexpr static auto duck_t = decay(^^Duck);
        constexpr static auto gen_t = substitute(^^vtable_generator,
            template_arguments_of(duck_t));
        return [:gen_t:]::template convert<Traits...[0]>(table);
    }
};
}

#endif
