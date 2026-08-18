// Copied from https://github.com/RyanJK5/rjk-flag/.

#ifndef RJK_FLAG_HPP
#define RJK_FLAG_HPP

#include <meta>

namespace rjk::detail {

template <typename Tag = decltype([] {}), bool On = true>
struct flag {
    consteval std::meta::info operator()(bool b) const {
        return substitute(^^rjk::detail::flag, {^^Tag, std::meta::reflect_constant(b)});
    }
};

template <typename T>
concept flag_type = (has_template_arguments(^^T) && template_of(^^T) == ^^flag);

consteval bool is_flag_set(std::meta::info entity, flag_type auto flag) {
    for (const auto annotation : annotations_of(entity)) {
        if (type_of(annotation) == dealias(^^std::meta::info)) {
            if (extract<std::meta::info>(annotation) == type_of(^^flag)) {
                return true;
            }
        } else if (decay(type_of(annotation)) == type_of(^^flag)) {
            return true;
        }
    }
    return false;
}

}

#endif // RJK_FLAG_HPP
