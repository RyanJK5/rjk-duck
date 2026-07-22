// clang-format off

#ifndef RJK_SUBSUMPTION_UTILS_HPP
#define RJK_SUBSUMPTION_UTILS_HPP

#include <meta>

#include "detail/vtable_generator.hpp"
#include "rjk/duck.hpp"

namespace rjk::detail {

consteval static bool is_duck_view(std::meta::info type) {
    return has_template_arguments(type)
        && is_type(type)
        && template_of(type) == ^^duck_view;
}

template <typename T, typename Duck>
concept valid_duck_and_type = (is_duck_type(^^Duck) &&
    std::decay_t<Duck>::duck_base_t::template meets_tags<T>());

template <duck_type SelfDuck, is_trait... Traits>
struct subsumption_utils {
    constexpr static std::array<std::meta::info, sizeof...(Traits)>
        traits{^^Traits...};

    using vtable_gen_t = vtable_generator<Traits...>;

    template <duck_type Duck>
    constexpr static bool can_convert_from = std::invoke([] {
        constexpr static auto duck_t = decay(^^Duck);
        constexpr static auto dest_traits = define_static_array(template_arguments_of(duck_t));
        using dest_gen_t = [: substitute(^^vtable_generator, dest_traits) :];
        using const_dest_gen_t = [: substitute(^^vtable_generator,
            dest_traits | std::views::transform(std::meta::add_const)) :];

        if constexpr (std::same_as<std::decay_t<Duck>, SelfDuck>) {
            return false;
        } else if constexpr (std::same_as<vtable_gen_t, dest_gen_t>) {
            return true;
        } else if constexpr (std::same_as<vtable_gen_t, const_dest_gen_t>) {
            return true;
        } else if constexpr (sizeof...(Traits) == 1UZ) {
            constexpr static auto self_trait = remove_const(traits[0UZ]);
            return std::ranges::any_of(dest_traits, [](auto t) {
                return remove_const(t) == self_trait;
            });
        } else {
            return false;
        }
    });

    template <typename Duck>
    constexpr static const vtable_generator<Traits...>::vtable* convert_from(const auto* table) {
        constexpr static auto duck_t = decay(^^Duck);
        using dest_gen_t = [: substitute(^^vtable_generator,
            template_arguments_of(duck_t)) :];
        using const_dest_gen_t = [: substitute(^^vtable_generator,
            template_arguments_of(duck_t) | std::views::transform(std::meta::add_const)) :];

        if constexpr (std::same_as<vtable_gen_t, dest_gen_t>) {
            return table;
        } else if constexpr (std::same_as<vtable_gen_t, const_dest_gen_t>) {
            return table->to_const;
        } else if constexpr (sizeof...(Traits) == 1UZ) {
            constexpr static auto gen_t = substitute(^^vtable_generator,
                template_arguments_of(duck_t));
            return [:gen_t:]::template convert<Traits...[0]>(table);
        } else {
            display_error("no conversion found");
        }
    }
};
}

#endif
