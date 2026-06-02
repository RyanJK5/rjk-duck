#include "duck.hpp"

#include <string>
#include <vector>
#include <deque>
#include <meta>

#include <gtest/gtest.h>

namespace rjk_test {
// Type-erased container: anything with size() and clear()
using Sizeable = rjk::duck<rjk::policy<
    rjk::has_fn<"size", std::size_t() const>,
    rjk::has_fn<"clear", void()>,
    rjk::has_fn<"empty", bool() const>
>>;
static_assert(
    rjk::remove_noexcept(type_of(^^std::vector<int>::size)) ==
    rjk::remove_noexcept(^^std::size_t() const));

TEST(StdlibScenarios, VectorSizeAndEmpty) {
    Sizeable x{std::vector<int>{1, 2, 3}};
    EXPECT_EQ(x.size(), 3UZ);
    EXPECT_FALSE(x.empty());
}

TEST(StdlibScenarios, StringSizeAndEmpty) {
    Sizeable x{std::string{"hello"}};
    EXPECT_EQ(x.size(), 5u);
    EXPECT_FALSE(x.empty());
}

TEST(StdlibScenarios, VectorClear) {
    Sizeable x{std::vector<int>{1, 2, 3}};
    x.clear();
    EXPECT_EQ(x.size(), 0u);
    EXPECT_TRUE(x.empty());
}

TEST(StdlibScenarios, MapSizeAndClear) {
    std::map<int, int> m{{1, 2}, {3, 4}};
    Sizeable x{std::move(m)};
    EXPECT_EQ(x.size(), 2u);
    x.clear();
    EXPECT_TRUE(x.empty());
}

TEST(StdlibScenarios, DequeInSizeable) {
    Sizeable x{std::deque<double>{1.0, 2.0}};
    EXPECT_EQ(x.size(), 2u);
    x.clear();
    EXPECT_TRUE(x.empty());
}

TEST(StdlibScenarios, SizeableSwap) {
    Sizeable x{std::vector<int>{1, 2, 3}};
    Sizeable y{std::string{"hi"}};
    std::swap(x, y);
    EXPECT_EQ(x.size(), 2u); // string "hi"
    EXPECT_EQ(y.size(), 3u); // vector {1,2,3}
}

TEST(StdlibScenarios, SizeableCopy) {
    Sizeable x{std::vector<int>{1, 2, 3}};
    Sizeable y{x};
    y.clear();
    EXPECT_EQ(x.size(), 3u); // x unaffected
    EXPECT_EQ(y.size(), 0u);
}

// Type-erased string-like: anything with find() and substr()-equivalent
// Use a narrower interface: just size() const and a find(char) method
using StringLike = rjk::duck<rjk::policy<
    rjk::has_fn<"size", std::size_t() const>,
    rjk::has_fn<"empty", bool() const>
>>;

TEST(StdlibScenarios, StringVsVectorChar) {
    StringLike a{std::string{"hello"}};
    StringLike b{std::vector<char>{'h', 'i'}};
    EXPECT_EQ(a.size(), 5u);
    EXPECT_EQ(b.size(), 2u);
    EXPECT_FALSE(a.empty());
    EXPECT_FALSE(b.empty());
}

// Type-erased output: anything callable as a sink via push_back
// Use back_inserter-compatible containers
using Pushable = rjk::duck<rjk::policy<
    rjk::has_fn<"push_back", void(const int&)>,
    rjk::has_fn<"size", std::size_t() const>
>>;

TEST(StdlibScenarios, VectorPushBack) {
    Pushable x{std::vector<int>{}};
    x.push_back(10);
    x.push_back(20);
    EXPECT_EQ(x.size(), 2u);
}

TEST(StdlibScenarios, DequePushBack) {
    Pushable x{std::deque<int>{}};
    x.push_back(1);
    x.push_back(2);
    x.push_back(3);
    EXPECT_EQ(x.size(), 3u);
}

TEST(StdlibScenarios, PushableMove) {
    Pushable x{std::vector<int>{1, 2}};
    Pushable y{std::move(x)};
    y.push_back(3);
    EXPECT_EQ(y.size(), 3u);
}

// Move-only stdlib type: unique_ptr wrapped in a custom holder
// (unique_ptr itself doesn't satisfy any useful tag directly,
//  so wrap it to verify move-only heap allocation works with real stdlib internals)
struct UniquePtrHolder {
    std::unique_ptr<std::vector<int> > data;

    explicit UniquePtrHolder(std::vector<int> v)
        : data(std::make_unique<std::vector<int> >(std::move(v))) {}

    std::size_t size() const { return data->size(); }
    bool empty() const { return data->empty(); }
    void clear() { data->clear(); }
};

TEST(StdlibScenarios, MoveOnlyStdlibWrapper) {
    Sizeable x{UniquePtrHolder{std::vector<int>{1, 2, 3, 4}}};
    EXPECT_EQ(x.size(), 4UZ);
    Sizeable y{std::move(x)};
    EXPECT_EQ(y.size(), 4UZ);
    y.clear();
    EXPECT_TRUE(y.empty());
}
}