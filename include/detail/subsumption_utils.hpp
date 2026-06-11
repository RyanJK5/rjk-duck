#ifndef RJK_SUBSUMPTION_UTILS_HPP
#define RJK_SUBSUMPTION_UTILS_HPP

#include <meta>

#include "duck_base.hpp"

namespace rjk::detail {

consteval static bool is_duck_type(std::meta::info type) {
    type = dealias(decay(type));

    if (!has_template_arguments(type)) {
        return false;
    }
    if (template_of(type) != ^^duck && template_of(type) != ^^duck_view) {
        return false;
    }
    return true;
}

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
        return std::ranges::equal(traits,
            template_arguments_of(type) | std::views::transform(std::meta::add_const)
        );
    }

    consteval static bool single_trait_subsumption(std::meta::info type) {
        if (!is_duck_type(type)) {
            return false;
        }

        const auto args = template_arguments_of(type);
        if (sizeof...(Traits) != 1 || (args.size() == 1 && is_const(type) == is_const(*traits.begin()))) {
            return false;
        }
        return std::ranges::contains(
            args | std::views::transform(std::meta::remove_const),
            remove_const(*traits.begin())
        );
    }

    template <typename Duck>
    static const vtable_generator<Traits...>::vtable* convert_from(const auto* table) {
        constexpr static auto duck_t = decay(^^Duck);
        constexpr static auto gen_t = substitute(^^vtable_generator,
            template_arguments_of(duck_t));
        return [:gen_t:]::template convert<Traits...[0]>(table);
    }
};
}

#endif // RJK_SUBSUMPTION_UTILS_HPP
