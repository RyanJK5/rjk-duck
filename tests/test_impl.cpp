#include "duck.hpp"
#include "duck_view.hpp"

#include <deque>
#include <fstream>
#include <filesystem>
#include <gtest/gtest.h>

struct [[=rjk::trait]] Serializable {
    auto Serialize() const -> std::string;
};

template <std::ranges::range T>
struct rjk::impl<T, Serializable> {
    constexpr static auto Serialize(const auto& self) -> std::string {
        std::string out{};
        for (auto& member : self) {
            out += 'a';
            // out += std::to_string(member);
            if (member != self.back()) {
                out += ",";
            }
        }
        return out;
    }
};

namespace rjk_test {

TEST(ImplSuite, VectorSerialize) {
    std::vector<int> data{1, 2, 3, 4, 5};
    rjk::duck_view<Serializable> view{data};
    EXPECT_EQ(view.Serialize(), "1,2,3,4,5");
}

TEST(ImplSuite, DequeSerialize) {
    std::deque<int> data{10, 20, 30};
    rjk::duck_view<const Serializable> view{data};
    EXPECT_EQ(view.Serialize(), "10,20,30");
}

TEST(ImplSuite, ArraySerialize) {
    std::array data{7, 8, 9};
    rjk::duck_view<const Serializable> view{data};
    EXPECT_EQ(view.Serialize(), "7,8,9");
}

}

struct [[=rjk::trait]] SerializableToFile : Serializable {
    auto Serialize(const std::filesystem::path& file) const -> void;
};

template <typename T>
struct rjk::impl<T, SerializableToFile> {
    static auto Serialize(const auto& self, const std::filesystem::path& file) -> void {
        std::ofstream out{file};
        out << rjk::duck_view<const Serializable>{self}.Serialize();
    }
};

TEST(ImplSuite, VectorSerializeToFile) {
    std::vector data{1, 2, 3, 4, 5};
    rjk::duck_view<SerializableToFile> view{data};
    auto outPath = std::filesystem::temp_directory_path() / "out.txt";
    view.Serialize(outPath);

    std::ifstream input{outPath};
    std::string token{};
    std::size_t index{};
    while (std::getline(input, token, ',')) {
        EXPECT_EQ(data[index++], std::stoi(token));
    }

    EXPECT_EQ(view.Serialize(), "1,2,3,4,5");
}

static_assert(rjk::duck_view<const Serializable>{std::array{'a','b','c'}}.Serialize() == "a,b,c");