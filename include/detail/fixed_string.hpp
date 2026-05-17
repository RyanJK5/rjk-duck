#pragma once

namespace rjk {
    template <std::size_t N>
    struct fixed_string {
        char data[N]{};

        consteval fixed_string(const char (&str)[N]) {
            std::copy_n(str, N, data);
        }

        consteval bool operator==(const fixed_string&) const = default;

        consteval operator std::string_view() const {
            return {data, N - 1};
        }
    };
}