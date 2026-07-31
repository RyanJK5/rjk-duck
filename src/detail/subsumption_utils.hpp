// clang-format off

#ifndef RJK_SUBSUMPTION_UTILS_HPP
#define RJK_SUBSUMPTION_UTILS_HPP

#include <meta>

#include "detail/vtable_generator.hpp"
#include "duck_tags.hpp"
#include "remove_fn_qualifiers.hpp"

#include <algorithm>
#include <concepts>
#include <meta>
#include <type_traits>

namespace rjk::detail {

consteval static bool is_duck_view(std::meta::info type) {
    type = decay(type);
    return has_template_arguments(type)
        && is_type(type)
        && template_of(type) == ^^duck_view;
}

template <typename T, typename Duck>
concept valid_duck_and_type = (is_duck_type(^^Duck) &&
    std::decay_t<Duck>::duck_base_t::template meets_tags<T>);

template <duck_type SelfDuck, typename... Traits>
struct subsumption_utils {
    constexpr static std::array<std::meta::info, sizeof...(Traits)>
        traits{^^Traits...};

    using base_gen_t = [: make_vtable_generator(^^SelfDuck) :];

    template <duck_type Duck>
    constexpr static bool is_permutation = std::invoke([] {
        constexpr auto duck_t = decay(^^Duck);

        if (duck_t == ^^SelfDuck) {
            return false;
        }

        using dest_gen = [: make_vtable_generator(duck_t) :];
        return std::same_as<base_gen_t, dest_gen>;
    });

    template <duck_type Duck>
    constexpr static bool can_convert_from = std::invoke([] {
        constexpr auto duck_t = decay(^^Duck);
        using dest_gen = [: make_vtable_generator(duck_t) :];
        using const_dest_gen = [: make_vtable_generator(
            template_arguments_of(duck_t)
            | std::views::transform(std::meta::add_const)
            | std::ranges::to<std::vector>()
        ) :];

        if (duck_t == ^^SelfDuck) {
            return false;
        } else if (std::same_as<base_gen_t, dest_gen>) {
            return true;
        } else if (std::same_as<base_gen_t, const_dest_gen>) {
            return true;
        } else if (sizeof...(Traits) == 1UZ) {
            constexpr static auto self_trait = remove_const(traits[0UZ]);
            return std::ranges::any_of(template_arguments_of(duck_t), [](auto t) {
                return remove_const(t) == self_trait;
            });
        } else {
            return false;
        }
    });

    template <typename Duck>
    constexpr static const vtable_generator<Traits...>::vtable* convert_from(const auto* table) {
        constexpr auto duck_t = decay(^^Duck);
        using dest_gen = [: make_vtable_generator(duck_t) :];
        using const_dest_gen = [: make_vtable_generator(
            template_arguments_of(duck_t)
            | std::views::transform(std::meta::add_const)
            | std::ranges::to<std::vector>()
        ) :];

        if constexpr (std::same_as<base_gen_t, dest_gen>) {
            return table;
        } else if constexpr (std::same_as<base_gen_t, const_dest_gen>) {
            return table->to_const;
        } else if constexpr (sizeof...(Traits) == 1UZ) {
            return dest_gen::template convert<Traits...[0]>(table);
        } else {
            display_error("no conversion found");
        }
    }
};
}

#endif
