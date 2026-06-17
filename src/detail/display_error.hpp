// clang-format off

#ifndef RJK_DISPLAY_ERROR_HPP
#define RJK_DISPLAY_ERROR_HPP

#include "detail/fixed_string.hpp"

namespace rjk::detail {
template <fixed_string Error>
consteval void display_error_impl() {
    static_assert(false, Error);
}

// Displays a static assertion based on non-constant branching. In general,
// we prefer this to compile-time exceptions for the slightly more readable
// error messages and the compatability with -fno-exceptions.
consteval void display_error(std::string_view message) {
    const auto message_refl = std::meta::reflect_constant(fixed_string{message});
    const auto error_func = substitute(^^display_error_impl, {message_refl});
    extract<void (*)()>(error_func);
}
}

#endif
