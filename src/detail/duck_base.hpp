// clang-format off

#ifndef RJK_DUCK_BASE_HPP
#define RJK_DUCK_BASE_HPP

#include <functional>
#include <meta>
#include <ranges>

#include "duck_tags.hpp"
#include "detail/vtable_generator.hpp"
#include "detail/vtable_caller.hpp"
#include "detail/vtable_fn_maker.hpp"
#include "detail/subsumption_utils.hpp"

namespace rjk {

template <is_trait... Traits>
class duck;

template <is_trait... Traits>
class duck_view;

namespace detail {

template <typename Derived, duck_tag... Tags>
class duck_base {
public:
protected:
    // Define context once, to be used throughout duck_base
    constexpr static auto ctx = std::meta::access_context::unprivileged();

    constexpr static bool can_copy = (std::same_as<Tags, copy_tag> || ...);

    using vtable_gen_t = [:
        substitute(^^vtable_generator, template_arguments_of(^^Derived))
    :];

    using vtable = vtable_gen_t::vtable;

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
    consteval static auto vtable_function_wrapper_for(std::meta::info tag) {
        if (template_of(tag) == ^^has_fn) {
            return substitute(^^vtable_function_wrapper, {template_arguments_of(tag)[0]});
        } else if (template_of(tag) == ^^has_op) {
            fixed_string str{op_tag_to_string(tag)};
            return substitute(^^vtable_function_wrapper, {std::meta::reflect_constant(str)});
        } else {
            display_error("bad tag");
        }
    }

    // The callable object that acts as the member function (myDuck.foo()).
    // It's syntax sugar for directly accessing the static vtable and placing
    // the duck in the first void* slot.
    template <std::meta::info VtableMember, duck_tag Tag, fn_qualifiers Qualifiers,
        typename Func>
    class vtable_function;

    template <std::meta::info VtableMember, duck_tag Tag, fn_qualifiers Qualifiers,
        bool Noexcept, typename Ret, typename... Args>
    class vtable_function<VtableMember, Tag, Qualifiers, Ret(Args...) noexcept(Noexcept)> {
    public:
        using vtable_function_wrapper_t = [: vtable_function_wrapper_for(^^Tag) :];

        constexpr Ret operator()(Args... args) noexcept(Noexcept)
            requires (Qualifiers == fn_qualifiers::none);

        constexpr Ret operator()(Args... args) & noexcept(Noexcept)
            requires (Qualifiers == fn_qualifiers::lvalue_ref);

        constexpr Ret operator()(Args... args) && noexcept(Noexcept)
            requires (Qualifiers == fn_qualifiers::rvalue_ref);

        constexpr Ret operator()(Args... args) const noexcept(Noexcept)
            requires (Qualifiers == fn_qualifiers::is_const);

        constexpr Ret operator()(Args... args) const & noexcept(Noexcept)
            requires (Qualifiers == (fn_qualifiers::is_const | fn_qualifiers::lvalue_ref));

        constexpr Ret operator()(Args... args) const && noexcept(Noexcept)
            requires (Qualifiers == (fn_qualifiers::is_const | fn_qualifiers::rvalue_ref));

        constexpr ~vtable_function() = default;
    private:
        constexpr vtable_function() noexcept = default;
        constexpr vtable_function(const vtable_function&) noexcept = default;
        constexpr vtable_function(vtable_function&&) noexcept = default;
        constexpr vtable_function& operator=(const vtable_function&) noexcept = default;
        constexpr vtable_function& operator=(vtable_function&&) noexcept = default;

        // These functions let us find the enclosing duck without having to
        // store a pointer to it.
        constexpr Derived& trace_to_duck() noexcept;
        constexpr const Derived& trace_to_duck() const noexcept;

        template <fixed_string TagIdentifier>
        friend struct vtable_function_wrapper;

        friend Derived;

        template <typename... Callables>
        friend struct overload_set;
    };
protected:
    consteval static std::meta::info generate_vtable_function(std::meta::info tag, std::meta::info vtable_member) {
        const auto full_sig = template_arguments_of(tag)[1];
        const auto sig = remove_fn_qualifiers(template_arguments_of(tag)[1]);

        auto qualifiers = is_duck_view(^^Derived)
            ? fn_qualifiers::is_const
            : qualifiers_of(full_sig);

        return substitute(^^vtable_function, {
            reflect_constant(vtable_member),
            tag,
            std::meta::reflect_constant(qualifiers),
            sig
        });
    }

    consteval static std::meta::info generate_vtable_operator(std::meta::info tag, std::meta::info vtable_member) {
        const auto [_, qualifiers, after_remove_self, _]
            = analyze_op_tag(tag);

        const auto name = op_tag_to_string(tag);

        const auto sig = remove_fn_qualifiers(after_remove_self);

        return substitute(^^vtable_function, {
            reflect_constant(vtable_member),
            tag,
            std::meta::reflect_constant(qualifiers),
            sig
        });
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
                    const auto value = value_generator();

                    // This case is relevant for a mutable duck_view. Since
                    // duck_view<Trait> uses shallow const, a const duck_view can only
                    // pick the non-const overload. To avoid an ambiguous call,
                    // we preemptively filter out the const overload. This is still
                    // safe for a duck_view<const Trait> since there can't be mutable
                    // overloads there in the first place.
                    if (is_duck_view(^^Derived) && vtable_gen_t::is_mutable) {
                        const auto new_tag = dealias(template_arguments_of(dealias(value))[1]);
                        const auto new_func_type = template_arguments_of(new_tag)[1];
                        const auto new_after_self = template_of(new_tag) == ^^has_op
                            ? analyze_op_tag(new_tag).after_remove_self
                            : new_func_type;
                        const auto new_qualifiers = template_of(new_tag) == ^^has_op
                            ? qualifiers_of_target(new_func_type, ^^self)
                            : qualifiers_of(new_func_type);

                        const bool new_is_const = static_cast<bool>(new_qualifiers & fn_qualifiers::is_const);

                        const auto matches_sig = [=](auto vtable_func) {
                            const auto existing_tag = dealias(template_arguments_of(dealias(vtable_func))[1]);
                            const auto existing_after_self = template_of(existing_tag) == ^^has_op
                                ? analyze_op_tag(existing_tag).after_remove_self
                                : template_arguments_of(existing_tag)[1];
                            return parameters_of(new_after_self)
                                == parameters_of(existing_after_self);
                        };

                        if (new_is_const) {
                            // Incoming is const, drop it if there's already a non-const match
                            if (std::ranges::any_of(it->second, matches_sig)) {
                                return;
                            }
                        } else {
                            // Incoming is non-const, erase any existing const match
                            std::erase_if(it->second, matches_sig);
                        }
                    }

                    it->second.push_back(value);
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

        template for (constexpr auto tag_index : std::views::indices(sizeof...(Tags))) {
            const auto tag = tags[tag_index];
            if (tag == ^^copy_tag) {
                continue;
            }

            const auto member = vtable_caller<vtable_gen_t>::get_callable(tag_index);

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
                        dealias(vtable_function_wrapper_for(tag)),
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
        const auto names = group_tags_by_name<false>();
        std::vector<std::meta::info> bases{};
        for (const auto& name : names) {
            const fixed_string fixed_str{name};
            bases.push_back(substitute(^^vtable_function_wrapper, {std::meta::reflect_constant(fixed_str)}));
        }

        return substitute(^^vtable_wrapper_impl, bases);
    }

    template <typename T>
    constexpr static bool meets_tags = std::invoke([] {
        if(can_copy && !std::copyable<std::decay_t<T>>) {
            return false;
        }

        const auto type = decay(^^T);
        const auto is_class = is_class_type(type) || is_union_type(type);
        for (const auto trait : vtable_gen_t::traits) {
            const auto tags = members_to_tags(trait);
            if (!is_class && std::ranges::any_of(tags, [](auto tag) {
                return has_template_arguments(tag) && template_of(tag) == ^^has_fn;
            })) {
                return false;
            }
            const auto satisfy_func = substitute(^^satisfies_tags,
                std::views::concat(
                    std::array{type, trait},
                    tags));
            if (!std::invoke(extract<bool(*)()>(satisfy_func))) {
                return false;
            }
        }
        return true;
    });

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

consteval std::meta::info make_duck_base(std::meta::info derived, std::initializer_list<std::meta::info> traits) {
    auto processed_tags = traits
        | std::views::transform(members_to_tags)
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

#endif
