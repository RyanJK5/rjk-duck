#ifndef RJK_FIXED_STRING_HPP
#define RJK_FIXED_STRING_HPP

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

    consteval bool operator==(const fixed_string&) const = default;

    template <std::size_t OtherN>
    consteval auto operator+(const fixed_string<OtherN>& other) const {
        fixed_string<N + OtherN - 1> result{};
        std::copy_n(data, N - 1, result.data);
        std::copy_n(other.data, OtherN - 1, result.data + N - 1);
        result.data[N + OtherN - 2] = '\0';

        return result;
    }

    consteval operator std::string_view() const {
        return {data, N - 1};
    }
};
}

#endif