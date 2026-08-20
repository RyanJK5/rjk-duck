// Copied from https://github.com/RyanJK5/rjk-flag/.

#ifndef RJK_FLAG_HPP
#define RJK_FLAG_HPP

#include <meta>

namespace rjk::detail {

template <typename Tag = decltype([] {})>
struct flag {
    bool on = true;

    consteval flag operator()(bool b) const {
        return flag{.on = b};
    }
};

template <typename T>
concept flag_type = (has_template_arguments(^^T) && template_of(^^T) == ^^flag);

template <flag_type Flag>
consteval bool is_flag_set(std::meta::info entity, Flag) {
    for (const auto annotation : annotations_of(entity)) {
        if (decay(type_of(annotation)) != ^^Flag) {
            continue;
        }

        const bool valid = extract<Flag>(annotation).on;
        if (valid) {
            return true;
        }
    }
    return false;
}

}  // namespace rjk

#endif // RJK_FLAG_HPP
