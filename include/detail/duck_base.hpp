#ifndef RJK_DUCK_BASE_HPP
#define RJK_DUCK_BASE_HPP

#include <functional>
#include <meta>
#include <map>
#include <ranges>

#include "duck_tags.hpp"
#include "vtable_fn_maker.hpp"
#include "substitute_fn_traits.hpp"

namespace rjk {

template <typename Policy>
class duck;

namespace detail {

template <duck_tag... Tags>
class storage;

// Commonly used for std::visit, but we can also use it to implement overloads
// by inheriting from vtable_functions.
template <typename... Callables>
struct overload_set : Callables... {
    using Callables::operator()...;
};

template <duck_tag... Tags>
class duck_base {
public:
    friend class storage<Tags...>;
protected:
    // Define context once, to be used throughout duck_base
    constexpr static auto ctx = std::meta::access_context::current();

    struct static_duck_vtable;

    // Converts '0' -> '_rjk__slot_0'
    consteval static std::string index_to_slot_name(std::size_t index) {
        std::string result{"_rjk__slot_"};
        if (index == 0UZ) return result + '0';
        std::string digits{};
        while (index > 0UZ) {
            digits += ('0' + index % 10UZ);
            index /= 10UZ;
        }
        std::ranges::reverse(digits);
        return result + digits;
    }

    // Defines the static_vtable struct. This contains all the function
    // pointers for a given duck, and accepts a type-erased void* as the first
    // argument, representing the object held within a duck.
    consteval {
        using StorageType = storage<Tags...>;
        std::vector<std::meta::info> members{ // special member functions
            data_member_spec(^^void(*)(StorageType&) noexcept, {.name = "destroy"}),
            data_member_spec(^^void(*)(StorageType&, StorageType&) noexcept, {.name = "move"}),
            data_member_spec(^^void(*)(const StorageType&, StorageType&), {.name = "copy"}),
        };

        auto index = 0UZ;
        template for (constexpr auto tag : {^^Tags...}) {
            constexpr static auto full_sig = template_arguments_of(tag)[1];

            if constexpr (template_of(tag) == ^^has_fn) {
                constexpr static auto erased_ptr_type =
                    analyze_op_sig(template_arguments_of(tag)[1], op_parentheses)
                    .erased_ptr_type;

                constexpr static auto sig = remove_noexcept(
                    remove_fn_qualifiers(full_sig));
                constexpr static auto ptr_type = substitute(^^fn_to_ptr_t,
                    {substitute(^^detail::prepend_arg_t, {erased_ptr_type, sig})});
                members.push_back(data_member_spec(ptr_type, {
                    .name = index_to_slot_name(index)
                }));
            }
            else if constexpr (template_of(tag) == ^^has_op) {
                constexpr static auto [_, qualifiers, after_remove_self,
                    erased_ptr_type] = analyze_op_tag(tag);

                constexpr static auto sig = normalized_sig(after_remove_self, ^^duck<policy<Tags...>>);
                constexpr static auto ptr_type = substitute(^^fn_to_ptr_t,
                    {substitute(^^detail::prepend_arg_t, {erased_ptr_type, sig})});
                members.push_back(data_member_spec(ptr_type, {
                    .name = index_to_slot_name(index)
                }));
            }
            index++;
        }

        define_aggregate(^^static_duck_vtable, members);
    }

    // The special functions, like move, copy, and destroy, are defined in
    // detail/storage.hpp.
    template <typename T>
    consteval static void set_storage_functions(static_duck_vtable& static_vtable);

    // Generates a static_vtable with the correct member functions for a type.
    template <typename T>
    constexpr static auto static_vtable_for = [] {
        static_duck_vtable table{};
        set_storage_functions<T>(table);

        constexpr static auto slots = define_static_array(
            nonstatic_data_members_of(^^duck_base<Tags...>::static_duck_vtable, ctx)
            | std::views::drop(3) // drop special members
        );

        // maybe_unused for the case where sizeof...(Tags) == 0
        [[maybe_unused]] constexpr static
            std::array<std::meta::info, sizeof...(Tags)> tags{^^Tags...};

        template for (constexpr auto index : std::views::indices(sizeof...(Tags))) {
            constexpr static auto tag = tags[index];

            if constexpr (template_of(tag) == ^^has_fn) {
                constexpr static auto member_name = template_arguments_of(tag)[0];
                constexpr static auto full_sig    = template_arguments_of(tag)[1];
                constexpr static auto qualifiers  = qualifiers_of(full_sig);

                constexpr static auto T_member = std::invoke([] -> std::meta::info {
                    template for (constexpr auto m : define_static_array(members_of(^^T, ctx))) {
                        if constexpr (has_identifier(m) && is_function(m)) {
                            if constexpr (identifier_of(m) == std::string_view{[:member_name:]}) {
                                if constexpr (dealias(remove_noexcept(type_of(m))) == remove_noexcept(full_sig)) {
                                    return m;
                                }
                            }
                        }
                    }
                    throw std::logic_error("Could not find member");
                });

                constexpr static auto sig = remove_noexcept(
                    remove_fn_qualifiers(full_sig));
                constexpr static auto fn_maker = substitute(^^vtable_fn_maker,
                    {sig, ^^qualifiers, ^^T_member, ^^T});

                table.[:slots[index]:] = [:fn_maker:]::make();
            }
            else if constexpr (template_of(tag) == ^^has_op) {
                constexpr static auto [op_kind, qualifiers, after_remove_self, _]
                    = analyze_op_tag(tag);
                constexpr static auto tag_op = template_arguments_of(tag)[0];

                constexpr static auto sig = normalized_sig(after_remove_self, ^^duck<policy<Tags...>>);

                constexpr static auto op_maker = substitute(^^vtable_op_maker,
                    {sig, std::meta::reflect_constant(qualifiers),
                        tag_op, std::meta::reflect_constant(op_kind), ^^T});

                table.[:slots[index]:] = [:op_maker:]::make();
            }
        }

        return table;
    }();
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
            constexpr static auto [fixed_t, str] = op_tag_to_fixed_string(tag);
            constexpr static typename [:fixed_t:] fixed_str{str};
            return substitute(^^vtable_function_wrapper, {std::meta::reflect_constant(fixed_str)});
        }
    }

    template <duck_tag Tag>
    using vtable_function_wrapper_for = [: vtable_function_wrapper_for_impl<Tag>() :];

    // The callable object that acts as the member function (myDuck.foo()).
    // It's syntax sugar for directly accessing the static vtable and placing
    // the duck in the first void* slot.
    template <std::size_t VtableIndex, fn_qualifiers Qualifiers, typename Func>
    class vtable_function;

    template <std::size_t VtableIndex, fn_qualifiers Qualifiers, typename Ret, typename... Args>
    class vtable_function<VtableIndex, Qualifiers, Ret(Args...)> {
    public:
        using vtable_function_wrapper_t = vtable_function_wrapper_for<Tags...[VtableIndex]>;

        constexpr static auto static_vtable_member = [] {
            const auto range = std::meta::nonstatic_data_members_of(^^static_duck_vtable, ctx);
            auto it = std::ranges::find_if(range,
                [](auto info) {
                    return identifier_of(info) == index_to_slot_name(VtableIndex);
                });
            if (it == range.end()) {
                throw std::logic_error("Could not find index in static_duck_vtable");
            }
            return *it;
        }();

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
        duck<policy<Tags...>>& trace_to_duck();
        const duck<policy<Tags...>>& trace_to_duck() const;

        template <fixed_string TagIdentifier>
        friend struct vtable_function_wrapper;
        friend class duck<policy<Tags...>>;

        template <typename... Callables>
        friend struct overload_set;
    };
protected:
    template <std::meta::info Tag, std::size_t Index>
    consteval static std::meta::info generate_vtable_function() {
        constexpr static auto full_sig = template_arguments_of(Tag)[1];
        constexpr static auto sig = remove_noexcept(remove_fn_qualifiers(template_arguments_of(Tag)[1]));
        constexpr static auto qualifiers = detail::qualifiers_of(full_sig);

        return substitute(^^vtable_function, {std::meta::reflect_constant(Index), ^^qualifiers, sig});
    }

    template <std::meta::info Tag, std::size_t Index>
    consteval static std::meta::info generate_vtable_operator() {
        constexpr static auto [_1, qualifiers, after_remove_self, _2]
            = analyze_op_tag(Tag);

        const auto name = op_tag_to_string(Tag);

        constexpr static auto sig = detail::normalized_sig(after_remove_self, ^^duck<policy<Tags...>>);

        return substitute(^^vtable_function,
            {std::meta::reflect_constant(Index), std::meta::reflect_constant(qualifiers), sig});
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
        const auto add_name = [&](const std::string& name, auto&& value_generator) {
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
        template for (constexpr auto index : std::views::indices(sizeof...(Tags))) {
            constexpr static auto tag = tags[index];

            if constexpr (template_of(tag) == ^^has_fn) {
                constexpr static std::string_view str{[: template_arguments_of(tag)[0] :]};
                auto name = std::string{str};
                add_name(name, generate_vtable_function<tag, index>);
            }
            else if constexpr (template_of(tag) == ^^has_op) {
                add_name(op_tag_to_string(tag), generate_vtable_operator<tag, index>);
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
            const auto name = std::invoke([] -> std::string {
                if constexpr (template_of(tag) == ^^has_fn) {
                    return std::string{[:template_arguments_of(tag)[0]:].data};
                }
                if constexpr (template_of(tag) == ^^has_op) {
                    return op_tag_to_string(tag);
                }
                throw std::logic_error("unknown tag");
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
            constexpr static auto [fixed_t, str] = make_fixed_string(name);
            constexpr static typename [:fixed_t:] fixed_str{str};
            bases.push_back(substitute(^^vtable_function_wrapper, {std::meta::reflect_constant(fixed_str)}));
        }

        return substitute(^^vtable_wrapper_impl, bases);
    }

    using vtable_wrapper = [: create_vtable_wrapper_impl() :];
};
}
}

#endif //RJK_DUCK_BASE_HPP
