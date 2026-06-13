#include "duck.hpp"
#include "duck_view.hpp"

#include <gtest/gtest.h>

namespace rjk_test {
    TEST(ReturnDeduction, BasicIntIterator) {
        struct [[=rjk::trait]] IntIterator {
            int& operator*();
            rjk::duck_view<IntIterator> operator++();
            rjk::duck_view<IntIterator> operator--();
        };

        std::vector data{5,6,7,8};
        rjk::duck<IntIterator> it{data.begin()};
        ASSERT_EQ(*it, 5);
        ASSERT_EQ(*(++it), 6);
        ++it;
        auto view = ++it;
        ++view;
        ASSERT_EQ(it.get<std::vector<int>::iterator>(), data.end());
    }

    TEST(ReturnDeduction, DataHolderIterator) {
        struct [[=rjk::trait]] DataHolder {
            int getData() const;
            void setData(int data);
        };

        struct Foo {
            int data{};
            int getData() const { return data; }
            void setData(int d) { data = d; }
        };

        struct [[=rjk::trait]] HolderIterator {
            rjk::duck_view<DataHolder> operator*();
            rjk::duck_view<HolderIterator> operator++();
            rjk::duck_view<HolderIterator> operator--();
        };

        std::vector data{Foo{5}, Foo{6}, Foo{7}, Foo{8}};
        rjk::duck<HolderIterator> it{data.begin()};
        ASSERT_EQ((*it).getData(), 5);
        ASSERT_EQ((*(++it)).getData(), 6);
        ++it;
        auto view = ++it;
        ++view;
        ASSERT_EQ(it.get<std::vector<Foo>::iterator>(), data.end());
    }

    template <rjk::is_trait Trait>
    struct [[=rjk::trait]] Clonable {
        rjk::duck<Trait> clone() const;
    };

    TEST(ReturnDeduction, Clonable) {
        struct [[=rjk::trait]] Fooable {
            int foo() const;
        };

        struct A {
            int foo() const { return 10; }
            A clone() const { return A{}; }
        };

        struct B {
            int foo() const { return 20; }
            B clone() const { return B{}; }
        };

        rjk::duck<Fooable, Clonable<Fooable>> d{A{}};
        EXPECT_EQ(d.foo(), 10);
        EXPECT_EQ(d.clone().foo(), 10);
        d = B{}.clone();
        EXPECT_EQ(d.foo(), 20);
        rjk::duck_view view{d};
        rjk::duck newDuck = view.clone();
        EXPECT_EQ(newDuck.foo(), 20);
    }

    TEST(ReturnDeduction, TakingXValue) {
        struct [[=rjk::trait]] MyTrait : rjk::copyable {
            int foo() const;
            int bar();
        };

        struct [[=rjk::trait]] DataHolder {
            rjk::duck_view<MyTrait> getData() &;
            rjk::duck_view<const MyTrait> getData() const &;
            rjk::duck<MyTrait> getData() &&;
        };

        struct A {
            int foo() const { return 10; }
            int bar() { return 20; }
        };

        struct B {
            A a{};

            A& getData() & { return a; }
            const A& getData() const& { return a; }

            A&& getData() && { return std::move(a); }
        };

        rjk::duck<DataHolder> d{B{}};
        rjk::duck d2{d.getData()};

        EXPECT_EQ(d2.foo(), 10);
        EXPECT_EQ(d2.bar(), 20);

        const rjk::duck d3{std::move(d)};
        rjk::duck<const MyTrait> d4{d3.getData()};

        EXPECT_EQ(d4.foo(), 10);
        // EXPECT_EQ(d4.bar(), 20);

        rjk::duck<DataHolder> d5{B{}};
        auto d6 = std::move(d5).getData();
        EXPECT_EQ(d6.foo(), 10);
        EXPECT_EQ(d6.bar(), 20);
    }

    template <typename Trait>
    struct [[=rjk::trait]] TraitIterator {
        rjk::duck_view<Trait> operator*() const;
        rjk::duck_ptr<Trait> operator->() const;

        rjk::duck_view<TraitIterator> operator++();
        rjk::duck_view<TraitIterator> operator--();
    };

    TEST(ReturnDeduction, FullIterator) {
        struct [[=rjk::trait]] Fooable {
            int foo() const;
            int bar();
        };

        struct Foo {
            int value{};
            int foo() const { return value; }
            int bar() { return value * 2; }
        };

        // TODO: The class templates must be instantiated before use, otherwise GCC emits
        // a circular dependency. Is this correct behavior or a bug?
        TraitIterator<Fooable>{};
        TraitIterator<const Fooable>{};

        std::vector data{Foo{1}, Foo{2}, Foo{3}, Foo{4}};
        rjk::duck<TraitIterator<Fooable>> it{data.begin()};
        EXPECT_EQ(it->foo(), 1);
        EXPECT_EQ((++it)->foo(), 2);

        it = data.end();
        EXPECT_EQ((*(--it)).foo(), 4);
        EXPECT_EQ(it->bar(), 8);

        const auto data2 = data;
        rjk::duck<TraitIterator<const Fooable>> it2{data2.begin()};
        EXPECT_EQ(it2->foo(), 1);
        it2 = --data2.end();
        EXPECT_EQ(it2->foo(), 4);
    }
}