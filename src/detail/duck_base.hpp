// clang-format off

#ifndef RJK_DUCK_BASE_HPP
#define RJK_DUCK_BASE_HPP

#include <functional>
#include <meta>
#include <ranges>

#include "duck_tags.hpp"
#include "meta_util.hpp"
#include "vtable_generator.hpp"
#include "detail/vtable_generator.hpp"
#include "detail/vtable_caller.hpp"
#include "detail/vtable_fn_maker.hpp"
#include "detail/subsumption_utils.hpp"

#include <meta>

namespace rjk::detail {

template <typename Derived, typename... Traits>
class duck_base {
public:
protected:
    // Define context once, to be used throughout duck_base
    constexpr static auto ctx = std::meta::access_context::unprivileged();

    using vtable_gen_t = vtable_generator<Traits...>;

    using util = subsumption_utils<Derived, Traits...>;

    using vtable = vtable_gen_t::vtable;

    template <typename T>
    constexpr static auto& static_vtable_for =
        vtable_gen_t::template static_vtable_for<T>;

protected:
    // Wraps a vtable_function with a name, so we can get myDuck.foo() syntax.
    template <fixed_string Identifier>
    struct vtable_function_wrapper;

    consteval static auto vtable_function_wrapper_for(std::meta::info member) {
        const auto name = pretty_name_of(member);
        const fixed_string str{std::string_view{name}};
        return substitute(^^vtable_function_wrapper, {std::meta::reflect_constant(str)});
    }

    // The callable object that acts as the member function (myDuck.foo()).
    // It's syntax sugar for directly accessing the static vtable and placing
    // the duck in the first void* slot.
    template <std::meta::info VtableMember, typename Wrapper, fn_qualifiers Qualifiers,
        typename Func>
    class vtable_function;

    template <std::meta::info VtableMember, typename Wrapper, fn_qualifiers Qualifiers,
        bool Noexcept, typename Ret, typename... Args>
    class vtable_function<VtableMember, Wrapper, Qualifiers, Ret(Args...) noexcept(Noexcept)> {
    public:
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

        template <fixed_string Identifier>
        friend struct vtable_function_wrapper;

        friend Derived;

        template <typename... Callables>
        friend struct overload_set;
    };
protected:
    consteval static std::meta::info make_wrapper(std::meta::info member, std::meta::info vtable_member) {
        const auto full_sig = type_of(member);
        const auto sig = remove_fn_qualifiers(full_sig);

        auto qualifiers = is_duck_view(^^Derived)
            ? fn_qualifiers::is_const
            : qualifiers_of(full_sig);

        return substitute(^^vtable_function, {
            reflect_constant(vtable_member),
            vtable_function_wrapper_for(member),
            std::meta::reflect_constant(qualifiers),
            sig
        });
    }

    // TODO: Rewrite using map / unordered_map once constexpr support is available
    consteval static std::vector<std::string> group_by_name() {
        std::vector<std::string> names{};
        template for (constexpr auto trait : vtable_gen_t::traits) {
            for (const auto member : members_for<typename [:trait:]>) {
                if (!is_user_provided(member)) {
                    continue;
                }

                const auto name = pretty_name_of(member);
                if (!std::ranges::contains(names, name)) {
                    names.push_back(name);
                }
            }
        }
        return names;
    }

    consteval static std::meta::info overload_set_for([[maybe_unused]] std::string_view name) {
        std::vector<std::meta::info> wrappers{};

        template for (constexpr auto trait_index : std::views::indices(sizeof...(Traits))) {
            std::vector<std::ptrdiff_t> const_indices{};
            bool has_mutable{false};
            const auto is_mutable_view = is_duck_view(^^Derived) &&
                !is_const(vtable_gen_t::traits[trait_index]);

            const auto& members = members_for<Traits...[trait_index]>;
            for (const auto [index, member] : std::views::enumerate(members)) {
                if (!is_user_provided(member) || pretty_name_of(member) != name) {
                    continue;
                }
                
                if (is_mutable_view) {
                    if (!is_const(member)) {
                        has_mutable = true;
                    } else if (!has_mutable) {
                        const_indices.push_back(index);
                    } else {
                        continue;
                    }
                }

                const auto vtable_member = vtable_caller<vtable_gen_t>
                    ::get_callable(trait_index, index);
                const auto wrapper = make_wrapper(member, vtable_member);
                wrappers.push_back(wrapper);
            }

            if (has_mutable) {
                for (const auto index : const_indices) {
                    wrappers.erase(wrappers.begin() + index);
                }
            }
        }

        return substitute(^^overload_set, wrappers);
    }

    // This generates a unique vtable_function_wrapper for each overload set
    // in the tags.
    consteval {
        for (const auto& name : group_by_name()) {
            const fixed_string fixed_str{name};
            const auto wrapper_type = substitute(^^vtable_function_wrapper, {
                std::meta::reflect_constant(fixed_str)
            });

            const auto overload_set = overload_set_for(name);
            const auto member_spec = data_member_spec(overload_set, {.name = name});
            define_aggregate(wrapper_type, {member_spec});
        }
    }

    // vtable_wrapper inherits from all of the vtable_function_wrappers we just
    // defined. When duck inherits from this class, it will therefore pull in
    // all of the .foo() and .bar() methods directly.
    template <typename... VtableFuncs>
    struct vtable_wrapper_impl : VtableFuncs... {};

    consteval static auto create_vtable_wrapper_impl() {
        const auto names = group_by_name();
        std::vector<std::meta::info> bases{};
        for (const auto& name : names) {
            const fixed_string fixed_str{name};
            bases.push_back(substitute(^^vtable_function_wrapper, {
                std::meta::reflect_constant(fixed_str)}));
        }

        return substitute(^^vtable_wrapper_impl, bases);
    }

    using vtable_wrapper = [: create_vtable_wrapper_impl() :];
};

consteval std::meta::info make_duck_base(std::meta::info derived) {
    auto traits = template_arguments_of(derived);

    std::ranges::sort(traits, [](auto a, auto b) {
        return std::is_lt(compare_meta_info(a, b));
    });

    return substitute(^^duck_base, std::views::concat(
        std::views::single(derived), traits)
    );
}

template <typename Duck>
using make_duck_base_t = [: make_duck_base(^^Duck) :];

}

#endif
