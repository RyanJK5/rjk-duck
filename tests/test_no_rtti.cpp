// Built with -fno-rtti: ducks without rjk::rtti don't need RTTI.
#include "rjk/duck.hpp"

#include <gtest/gtest.h>

namespace rjk_test {
namespace {

struct Named {
    auto name() const -> int;
};

struct Alpha {
    auto name() const -> int { return 1; }
};

template <typename Duck>
concept has_rtti_access = requires(Duck d) {
    rjk::get_if<Alpha>(&d);
    rjk::get<Alpha>(d);
    rjk::typeid_of(d);
};

static_assert(!has_rtti_access<rjk::duck<Named>>);
static_assert(!has_rtti_access<rjk::duck_view<const Named>>);

TEST(NoRtti, DucksWork) {
    rjk::duck<Named, rjk::copyable> d{Alpha{}};
    auto copy = d;
    rjk::duck_view<const Named> view{copy};
    EXPECT_EQ(view.name(), 1);
    EXPECT_EQ(d.name(), 1);
}

} // namespace
} // namespace rjk_test
