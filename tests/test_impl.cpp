#include "duck.hpp"
#include "duck_view.hpp"

#include <gtest/gtest.h>

struct [[=rjk::trait]] Serializable {
    auto Serialize() const -> std::string;
};

template <std::ranges::range T>
struct rjk::impl<T, Serializable> {
    static auto Serialize(const auto& self) -> std::string {
        std::string out{};
        for (auto& member : self) {
            out += std::to_string(member);
            if (member != self.back()) {
                out += ",";
            }
        }
        return out;
    }
};

namespace rjk_test {

TEST(ImplSuite, VectorSerialize) {
    std::vector<int> data{1,2,3,4,5};
    rjk::duck_view<Serializable> view{data};
    EXPECT_EQ(view.Serialize(), "1,2,3,4,5");
}

}