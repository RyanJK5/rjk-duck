#ifndef RJK_FLAG_ENUM_HPP
#define RJK_FLAG_ENUM_HPP

#include <bit>

namespace rjk::detail {

// Use in an attribute like [[=detail::flag_enum]] to mark that an enumeration
// type consists only of flags.
constexpr inline struct{} flag_enum{};

template <typename T>
concept is_flag_enum = std::is_enum_v<T> && requires { T::none == static_cast<T>(0); } && [] consteval {
    return std::ranges::any_of(annotations_of(^^T), [] (auto annotation) {
        return type_of(annotation) == type_of(^^flag_enum);
    });
}();

// Now every type with the flag_enum attribute gets operator| and operator& for free

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
}

#endif