#ifndef RJK_DUCK_BASE_HPP
#define RJK_DUCK_BASE_HPP

#include <functional>
#include <meta>
#include <map>
#include <ranges>

#include "duck_tags.hpp"
#include "duck_vtable_generator.hpp"
#include "vtable_fn_maker.hpp"
#include "substitute_fn_traits.hpp"
#include "display_error.hpp"

namespace rjk {

template <is_trait... Traits>
class duck;

template <is_trait... Traits>
class duck_view;

namespace detail {

// Commonly used for std::visit, but we can also use it to implement overloads
// by inheriting from vtable_functions.
template <typename... Callables>
struct overload_set : Callables... {
    using Callables::operator()...;
};

template <typename Derived, duck_tag... Tags>
class duck_base {
public:
protected:
    // Define context once, to be used throughout duck_base
    constexpr static auto ctx = std::meta::access_context::unprivileged();

    constexpr static bool can_copy = (std::same_as<Tags, copy_tag> || ...);

    using vtable_gen_t = [:
        substitute(^^duck_vtable_generator, std::views::concat(std::array{
            substitute(^^duck, template_arguments_of(^^Derived)),
            substitute(^^duck_view, template_arguments_of(^^Derived)),
        }, std::array<std::meta::info, sizeof...(Tags)>{^^Tags...}))
    :];

    using static_duck_vtable = vtable_gen_t::static_duck_vtable;

    template <typename T>
    constexpr static auto& static_vtable_for =
        vtable_gen_t::template static_vtable_for<T>;

protected:
    // Wraps a vtable_function with a name, so we can get myDuck.foo() syntax.
    // Accepts a fixed_string instead of tag because it is unique on overload
    // sets, but not on individual overloads.
    template <fixed_string TagIdentifier>
    struct vtable_function_wrapper;

    // Returns the vtable_function_wrapper for a tag.
    template <duck_tag Tag>
    consteval static auto vtable_function_wrapper_for_impl() {
        constexpr static auto tag = ^^Tag;

        if constexpr(template_of(tag) == ^^has_fn) {
            return substitute(^^vtable_function_wrapper, {template_arguments_of(tag)[0]});
        }
        else if constexpr(template_of(tag) == ^^has_op) {
            constexpr static auto fixed_str = op_tag_to_fixed_string(tag);
            return substitute(^^vtable_function_wrapper, {std::meta::reflect_constant(fixed_str)});
        }
    }

    template <duck_tag Tag>
    using vtable_function_wrapper_for = [: vtable_function_wrapper_for_impl<Tag>() :];

    // The callable object that acts as the member function (myDuck.foo()).
    // It's syntax sugar for directly accessing the static vtable and placing
    // the duck in the first void* slot.
    template <std::meta::info VtableMember, duck_tag Tag, fn_qualifiers Qualifiers, typename Func>
    class vtable_function;

    template <std::meta::info VtableMember, duck_tag Tag, fn_qualifiers Qualifiers, typename Ret, typename... Args>
    class vtable_function<VtableMember, Tag, Qualifiers, Ret(Args...)> {
    public:
        using vtable_function_wrapper_t = vtable_function_wrapper_for<Tag>;

        Ret operator()(Args... args) requires (Qualifiers == fn_qualifiers::none);

        Ret operator()(Args... args) & requires (Qualifiers == fn_qualifiers::lvalue_ref);

        Ret operator()(Args... args) && requires (Qualifiers == fn_qualifiers::rvalue_ref);

        Ret operator()(Args... args) const requires (Qualifiers == fn_qualifiers::is_const);

        Ret operator()(Args... args) const & requires (Qualifiers == (fn_qualifiers::is_const | fn_qualifiers::lvalue_ref));

        Ret operator()(Args... args) const && requires (Qualifiers == (fn_qualifiers::is_const | fn_qualifiers::rvalue_ref));

        ~vtable_function() = default;
    private:
        constexpr vtable_function() noexcept = default;
        vtable_function(const vtable_function&) noexcept = default;
        vtable_function(vtable_function&&) noexcept = default;
        vtable_function& operator=(const vtable_function&) noexcept = default;
        vtable_function& operator=(vtable_function&&) noexcept = default;

        // These functions let us find the enclosing duck without having to
        // store a pointer to it.
        Derived& trace_to_duck();
        const Derived& trace_to_duck() const;

        template <fixed_string TagIdentifier>
        friend struct vtable_function_wrapper;

        friend Derived;

        template <typename... Callables>
        friend struct overload_set;
    };
protected:
    consteval static std::meta::info generate_vtable_function(std::meta::info tag, std::meta::info vtable_member) {
        const auto full_sig = template_arguments_of(tag)[1];
        const auto sig = remove_noexcept(remove_fn_qualifiers(template_arguments_of(tag)[1]));
        const auto qualifiers = detail::qualifiers_of(full_sig);

        return substitute(^^vtable_function, {std::meta::reflect_constant(vtable_member),
            tag, std::meta::reflect_constant(qualifiers), sig});
    }

    consteval static std::meta::info generate_vtable_operator(std::meta::info tag, std::meta::info vtable_member) {
        const auto [_, qualifiers, after_remove_self, _]
            = analyze_op_tag(tag);

        const auto name = op_tag_to_string(tag);

        const auto sig = detail::normalized_sig(after_remove_self, ^^Derived);

        return substitute(^^vtable_function,
            {std::meta::reflect_constant(vtable_member), tag, std::meta::reflect_constant(qualifiers), sig});
    }

    // TODO: Rewrite using map / unordered_map once constexpr support is available
    template <bool FillInInfo>
    consteval static auto group_tags_by_name() {
        auto name_to_members = std::invoke([] consteval {
            if constexpr (FillInInfo) {
                return std::vector<std::pair<std::string, std::vector<std::meta::info>>>{};
            } else {
                return std::vector<std::string>{};
            }
        });
        const auto find_from_name = [&](const std::string& name) {
            return std::ranges::find_if(name_to_members, [&name](const auto& element) {
                if constexpr(FillInInfo) {
                    return element.first == name;
                } else {
                    return element == name;
                }
            });
        };
        [[maybe_unused]] const auto add_name = [&](const std::string& name, auto&& value_generator) {
            if (const auto it = find_from_name(name); it != name_to_members.end()) {
                if constexpr (FillInInfo) {
                    it->second.push_back(value_generator());
                }
            } else {
                if constexpr (FillInInfo) {
                    name_to_members.push_back({name, std::vector<std::meta::info>{}});
                    name_to_members.back().second.push_back(value_generator());
                } else {
                    name_to_members.push_back(name);
                }
            }
        };

        [[maybe_unused]] constexpr static
            std::array<std::meta::info, sizeof...(Tags)> tags{^^Tags...};

        [[maybe_unused]] const auto members = nonstatic_data_members_of(^^static_duck_vtable, ctx)
            | std::views::drop(can_copy ? 3 : 2);

        [[maybe_unused]] auto member_index = 0UZ;
        template for (constexpr auto tag_index : std::views::indices(sizeof...(Tags))) {
            const auto tag = tags[tag_index];
            if (tag == ^^copy_tag) {
                continue;
            }

            const auto member = members[member_index];
            member_index++;

            if (template_of(tag) == ^^has_fn) {
                const std::string_view str{extract<fixed_string>(template_arguments_of(tag)[0])};
                add_name(std::string{str},
                    [&] { return generate_vtable_function(tag, member); });
            }
            else if (template_of(tag) == ^^has_op) {
                add_name(op_tag_to_string(tag),
                    [&] { return generate_vtable_operator(tag, member); });
            }
        }

        return name_to_members;
    }

    // This generates a unique vtable_function_wrapper for each overload set
    // in the tags.
    consteval {
        [[maybe_unused]] constexpr static
            std::array<std::meta::info, sizeof...(Tags)> tags{^^Tags...};

        // First, collect all tags based on their name.
        auto name_to_members = group_tags_by_name<true>();

        // Create an overload_set for each name, and define vtable_function_wrapper
        // with an overload_set member.
        template for (constexpr auto tag : tags) {
            if constexpr (tag != ^^copy_tag) {
                const auto name = std::invoke([] -> std::string {
                    if constexpr (template_of(tag) == ^^has_fn) {
                        return std::string{[:template_arguments_of(tag)[0]:].data()};
                    }
                    if constexpr (template_of(tag) == ^^has_op) {
                        return op_tag_to_string(tag);
                    }
                });

                const auto it = std::ranges::find_if(name_to_members, [&name](const auto& pair) {
                    return pair.first == name;
                });
                if (it != name_to_members.end()) {
                    define_aggregate(
                        dealias(substitute(^^vtable_function_wrapper_for, {tag})),
                        {data_member_spec(substitute(^^overload_set, it->second),
                            {.name = it->first, .no_unique_address = true})}
                    );
                    name_to_members.erase(it);
                }
            }
        }
    }

    // vtable_wrapper inherits from all of the vtable_function_wrappers we just
    // defined. When duck inherits from this class, it will therefore pull in
    // all of the .foo() and .bar() methods directly.
    template <typename... VtableFuncs>
    struct vtable_wrapper_impl : VtableFuncs... {};

    consteval static auto create_vtable_wrapper_impl() {
        constexpr static auto names = define_static_array(
           group_tags_by_name<false>() | std::views::transform([](const auto& str) { return define_static_string(str); }));
        std::vector<std::meta::info> bases{};
        template for (constexpr auto name : names) {
            constexpr static fixed_string fixed_str{name};
            bases.push_back(substitute(^^vtable_function_wrapper, {std::meta::reflect_constant(fixed_str)}));
        }

        return substitute(^^vtable_wrapper_impl, bases);
    }

    template <typename T>
    consteval static bool meets_tags() {
        static_assert(!can_copy || std::copyable<std::decay_t<T>>,
            "duck was specified with rjk::copyable but T is not"
            " copyable");
        return satisfies_tags<std::decay_t<T>, Derived, Tags...>();
    }

    template <std::meta::operators Op, typename Lhs, typename Rhs>
    consteval static bool satisfies_operator(op_overload_kind kind) noexcept {
        if (!has_operator_tag<Op, Tags...>(kind)) {
            return false;
        }

        switch (kind) {
            using enum op_overload_kind;
        case any_kind:
            return true;
        case variadic:
            return true;
        case binary_lhs:
            return std::same_as<std::decay_t<Lhs>, Derived>;
        case binary_rhs:
            return std::same_as<std::decay_t<Rhs>, Derived> && !std::same_as<std::decay_t<Lhs>, std::decay_t<Rhs>>;
        case unary:
            return std::same_as<std::decay_t<Lhs>, Derived>;
        }
        return false;
    }

    using vtable_wrapper = [: create_vtable_wrapper_impl() :];
};

consteval bool is_const_tag(std::meta::info tag) {
    if (template_of(tag) == ^^has_fn) {
        return static_cast<bool>(qualifiers_of(template_arguments_of(tag)[1]) & fn_qualifiers::is_const);
    } else if (template_of(tag) == ^^has_op) {
        auto qualifiers = analyze_op_tag(tag).qualifiers;
        return static_cast<bool>(qualifiers & fn_qualifiers::is_const);
    } else {
        return true;
    }
};

consteval std::meta::info make_rhs_signature(std::meta::info member) {
    auto self_t = ^^self;
    if (is_const(member)) {
        self_t = add_const(self_t);
    }
    if (is_lvalue_reference_qualified(member)) {
        self_t = add_lvalue_reference(self_t);
    } else if (is_rvalue_reference_qualified(member)) {
        self_t = add_rvalue_reference(self_t);
    }

    const auto base_func_t = remove_fn_qualifiers(type_of(member));
    const auto with_self = dealias(substitute(^^append_arg_t, {self_t, base_func_t}));
    return substitute(^^has_op, {std::meta::reflect_constant(operator_of(member)), with_self});
}

consteval auto members_to_tags(std::meta::info trait) {
        if (extract<bool>(substitute(^^is_policy, {trait}))) {
            return template_arguments_of(trait);
        }

        constexpr static auto ctx = std::meta::access_context::unprivileged();
        auto trait_tags = members_of(remove_const(trait), ctx)
            | std::views::filter([trait](auto member) {
                if (is_function(member) && !is_user_declared(member)) {
                    return false;
                }
                if (is_function(member) && has_identifier(member)) {
                    return true;
                }
                if (is_operator_function(member)) {
                    return true;
                }
                display_error(std::string{"Trait '"} + display_string_of(trait)
                    + "' cannot hold non-member function '" + display_string_of(member) + "'");
                return false;
            })
            | std::views::transform([](auto member) -> std::vector<std::meta::info> {
                if (is_operator_function(member)) {
                    if (has_annotation(member, ^^rhs_op)) {
                        return {make_rhs_signature(member)};
                    }

                    const auto lhs_sig = substitute(^^has_op,
                        {std::meta::reflect_constant(operator_of(member)), type_of(member)});

                    if (has_annotation(member, ^^both_sides)) {
                        return {lhs_sig, make_rhs_signature(member)};
                    }

                    return {lhs_sig};
                } else if (is_function(member)) {
                    const fixed_string fixed_str{identifier_of(member)};
                    return {substitute(^^has_fn, {std::meta::reflect_constant(fixed_str), type_of(member)})};
                } else {
                    throw std::logic_error{"cannot handle member kind"};
                }
            })
            | std::views::join
            | std::ranges::to<std::vector>();

        auto base_tags = bases_of(trait, ctx)
            | std::views::transform([](auto base) { return type_of(base); })
            | std::views::transform(members_to_tags)
            | std::views::join;

        trait_tags.append_range(base_tags);
        return trait_tags;
    }

consteval std::meta::info make_duck_base(std::meta::info derived, std::initializer_list<std::meta::info> traits) {
    auto processed_tags = traits
        | std::views::transform([](auto trait) {
            auto tags = members_to_tags(dealias(trait));

            if (!is_const(trait)) {
                return tags;
            }

            return tags
                | std::views::filter(is_const_tag)
                | std::ranges::to<std::vector>();
        })
        | std::views::join;

    return substitute(^^duck_base, std::views::concat(
        std::views::single(derived),
        processed_tags
    ));
}

template <typename Derived, is_trait... Traits>
using make_duck_base_t = [: make_duck_base(^^Derived, {^^Traits...}) :];
}
}

#endif //RJK_DUCK_BASE_HPP
