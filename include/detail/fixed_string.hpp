#ifndef RJK_FIXED_STRING_HPP
#define RJK_FIXED_STRING_HPP

#include <meta>

namespace rjk {

// Compile-time string primitive. We need this to be able to pass
// strings as template arguments.
struct fixed_string {
    consteval fixed_string() = default;

    template <std::size_t N>
    consteval fixed_string(const char (&str)[N]) {
        m_data = std::define_static_string(str);
        m_length = N - 1;
    }

    consteval explicit fixed_string(std::string_view str) {
        m_data = std::define_static_string(str);
        m_length = str.size();
    }

    consteval bool operator==(const fixed_string&) const = default;

    consteval auto operator+(const fixed_string& other) const {
        std::string result_str{m_data};
        result_str += other.m_data;

        return fixed_string{std::string_view{result_str}};
    }

    consteval const char* data() const { return m_data; }
    consteval std::size_t size() const { return m_length; }

    consteval explicit operator std::string_view() const {
        return {m_data, m_length};
    }

    const char* m_data{};
    std::size_t m_length{};
};
}

#endif