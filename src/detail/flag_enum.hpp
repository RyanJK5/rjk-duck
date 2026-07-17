// clang-format off

#ifndef RJK_FLAG_ENUM_HPP
#define RJK_FLAG_ENUM_HPP

#include <ranges>
#include <utility>

namespace rjk::detail {

struct flag_enum_t{};
// Use in an attribute like [[=detail::flag_enum]] to mark that an enumeration
// type consists only of flags.
constexpr inline flag_enum_t flag_enum{};

consteval bool has_annotation(std::meta::info obj, std::meta::info annotation) {
    return std::ranges::any_of(annotations_of(obj),
        [annotation](auto a) { return type_of(a) == type_of(annotation); });
}

template <typename T>
concept is_flag_enum = std::is_enum_v<T> && requires { T::none == static_cast<T>(0); }
    && has_annotation(^^T, ^^flag_enum);

// Now every type with the flag_enum attribute gets the following operators for free.

template <is_flag_enum EnumType>
constexpr EnumType operator|(EnumType lhs, EnumType rhs) {
    return static_cast<EnumType>(std::to_underlying(lhs) | std::to_underlying(rhs));
}

template <is_flag_enum EnumType>
constexpr EnumType& operator|=(EnumType& lhs, EnumType rhs) {
    lhs = lhs | rhs;
    return lhs;
}

template <is_flag_enum EnumType>
constexpr EnumType operator&(EnumType lhs, EnumType rhs) {
    return static_cast<EnumType>(std::to_underlying(lhs) & std::to_underlying(rhs));
}

template <is_flag_enum EnumType>
constexpr EnumType& operator&=(EnumType& lhs, EnumType rhs) {
    lhs = lhs & rhs;
    return lhs;
}

template <is_flag_enum EnumType>
constexpr EnumType operator~(EnumType operand) {
    return static_cast<EnumType>(~std::to_underlying(operand));
}
}

#endif