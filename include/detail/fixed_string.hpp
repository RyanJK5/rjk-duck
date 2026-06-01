#ifndef RJK_FIXED_STRING_HPP
#define RJK_FIXED_STRING_HPP

#include <meta>

namespace rjk {

// Compile-time string primitive. We need this to be able to pass
// strings as template arguments.
struct fixed_string {
    const char* data{};
    std::size_t length{};

    consteval fixed_string() = default;

    template <std::size_t N>
    consteval fixed_string(const char (&str)[N]) {
        data = std::define_static_string(str);
        length = N - 1;
    }


    consteval explicit fixed_string(std::string_view str) {
        data = std::define_static_string(str);
        length = str.size();
    }

    consteval bool operator==(const fixed_string&) const = default;

    consteval auto operator+(const fixed_string& other) const {
        std::string result_str{data};
        result_str += other.data;

        return fixed_string{std::string_view{result_str}};
    }

    consteval explicit operator std::string_view() const {
        return {data, length};
    }
};
}

#endif