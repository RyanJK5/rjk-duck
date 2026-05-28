#ifndef RJK_FIXED_STRING_HPP
#define RJK_FIXED_STRING_HPP

#include <meta>

namespace rjk {

// Compile-time string primitive. We need this to be able to pass
// strings as template arguments.
template <std::size_t N>
struct fixed_string {
    char data[N]{};

    consteval fixed_string() = default;

    consteval fixed_string(const char (&str)[N]) {
        std::copy_n(str, N, data);
    }

    consteval explicit fixed_string(std::string_view str) {
        std::copy_n(str.data(), N, data);
    }

    consteval bool operator==(const fixed_string&) const = default;

    template <std::size_t OtherN>
    consteval auto operator+(const fixed_string<OtherN>& other) const {
        fixed_string<N + OtherN - 1> result{};
        std::copy_n(data, N - 1, result.data);
        std::copy_n(other.data, OtherN - 1, result.data + N - 1);
        result.data[N + OtherN - 2] = '\0';

        return result;
    }

    consteval explicit operator std::string_view() const {
        return {data, N - 1};
    }
};

struct fixed_result {
    std::meta::info fixed_t;
    std::string_view str;
};

consteval fixed_result make_fixed_string(const std::string& str) {
    return fixed_result{
        .fixed_t = substitute(^^rjk::fixed_string, {std::meta::reflect_constant(str.size() + 1UZ)}),
        .str = std::define_static_string(str)
    };
}
}

#endif