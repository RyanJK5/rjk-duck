#ifndef RJK_DUCK_UTILS_HPP
#define RJK_DUCK_UTILS_HPP

#include "detail/fixed_string.hpp"
#include "detail/flag_enum.hpp"
#include "duck_utils.hpp"

namespace rjk {

template <typename Type, typename RelevantTrait, typename... Tags>
consteval bool satisfies_tags();

template <typename T>
concept function_signature = std::is_function_v<T>;

using enum std::meta::operators;

template <typename E, bool Enumerable = std::meta::is_enumerable_type(^^E)>
    requires std::is_enum_v<E>
constexpr std::string_view enum_to_string(E value) {
    if constexpr (Enumerable) {
        template for (constexpr auto e : std::define_static_array(std::meta::enumerators_of(^^E))) {
            if (value == [:e:]) {
                return std::meta::identifier_of(e);
            }
        }
    } else {
        return "<unnamed>";
    }
}

template <fixed_string Identifier, function_signature Func>
struct has_fn {};

template <std::meta::operators Operator, function_signature Func>
struct has_op {};

struct copy_tag{};

// Used for denoting the relative location of two ducks in a has_op signature.
struct self{};

// Can be plugged into rjk::policy.
template <typename T>
concept duck_tag = std::same_as<T, copy_tag> || (has_template_arguments(^^T) && (
    template_of(^^T) == ^^has_fn ||
    template_of(^^T) == ^^has_op));

// Plugged into rjk::duck.
template <duck_tag... Tags>
struct policy{};

template <typename Func>
concept is_meta_predicate = std::invocable<Func, std::meta::info> &&
    std::same_as<std::invoke_result_t<Func, std::meta::info>, bool>;

// Models a trait based on the type's public interface and the provided predicate.
template <typename T, is_meta_predicate auto Predicate =
    [] (std::meta::info) consteval { return true; }>
struct like {};

template <is_meta_predicate auto... Predicates>
constexpr inline auto all_of = [](std::meta::info member) {
    return (std::invoke(Predicates, member) && ...);
};

template <is_meta_predicate auto... Predicates>
constexpr inline auto any_of = [](std::meta::info member) {
    return (std::invoke(Predicates, member) || ...);
};

template <is_meta_predicate auto... Predicates>
constexpr inline auto none_of = [](std::meta::info member) {
    return !(std::invoke(Predicates, member) || ...);
};

template <fixed_string... Whitelist>
constexpr inline auto include = [](std::meta::info member) {
    if (!has_identifier(member)) {
        return false;
    }
    const auto whitelist = std::array<fixed_string, sizeof...(Whitelist)>{Whitelist...}
        | std::views::transform([](auto str) {
            return std::string_view{str};
        });
    return std::ranges::contains(whitelist, identifier_of(member));
};

template <fixed_string... Blacklist>
constexpr inline auto exclude = [](std::meta::info member) {
    if (!has_identifier(member)) {
        return true;
    }

    const auto blacklist = std::array<fixed_string, sizeof...(Blacklist)>{Blacklist...}
        | std::views::transform([](auto str) {
            return std::string_view{str};
        });
    return !std::ranges::contains(blacklist, identifier_of(member));
};

// Passed as a policy to rjk::duck to allow copying.
using copyable = policy<copy_tag>;

// The following are meant to be used as attributes.

// [[=rjk::trait]] specifies that a struct will be used as a trait.
constexpr inline struct{} trait{};

// [[=rjk::right_side]] specifies that an operator function is being defined with self as the last argument.
constexpr inline struct{} right_side{};

// [[=rjk::both_sides]] specifies that an operator function needs both self + T and T + self.
constexpr inline struct{} both_sides{};

// [[=rjk::perf_options]] specifies a trait that changes the default performance options for
// rjk::duck. Currently, these means customizing sbo_size and sbo_alignment.
constexpr inline struct{} perf_options{};

template <typename T>
concept is_policy = (has_template_arguments(^^T) && template_of(^^T) == ^^policy);

template <typename T>
concept is_like = (has_template_arguments(^^T) && template_of(^^T) == ^^like);

template <typename T>
concept is_perf_option = detail::has_annotation(^^T, ^^perf_options);

template <typename T>
concept is_trait = (
    is_policy<T> || is_like<T> || is_perf_option<T> ||
    detail::has_annotation(^^T, ^^trait));

template <is_trait... Traits>
class duck;

template <is_trait... Traits>
class duck_view;

template <is_trait... Traits>
class duck_ptr;

// Allows users to add additional methods to existing types.
template <typename T, is_trait Trait>
struct impl {};

// Various rules for member function lookup.
enum struct [[=detail::flag_enum]] lookup_rule {
    none = 0,
    strict = 1
};
}

#endif