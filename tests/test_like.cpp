#include <gtest/gtest.h>

#include "duck.hpp"
#include "duck_view.hpp"

namespace rjk_test {

class MyInterface {
public:
    virtual ~MyInterface() = default;

    virtual int foo() = 0;

    virtual int bar() = 0;

    virtual int buzz() {
        return 25;
    }
};

class TestA : public MyInterface {
    int foo() override { return 1; }
    int bar() override { return 2; }
};

struct TestB {
    int foo() { return 3; }
    int bar() { return 4; }
    int buzz() { return 30; }
};

TEST(LikeTrait, Basic) {
    rjk::duck<rjk::like<MyInterface>> d{TestA{}};
    EXPECT_EQ(d.foo(), 1);
    EXPECT_EQ(d.buzz(), 25);
    d = TestB{};
    EXPECT_EQ(d.bar(), 4);
    EXPECT_EQ(d.buzz(), 30);

    rjk::duck<rjk::like<MyInterface, std::meta::is_pure_virtual>> d2{TestA{}};
    EXPECT_EQ(d2.foo(), 1);
    // d2.buzz(); ERROR
}

}