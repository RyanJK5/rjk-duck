#include <gtest/gtest.h>
#include <numeric>

#include "duck.hpp"
#include "duck_view.hpp"

namespace rjk_test {
struct Foo {
    int data;
};

struct [[=rjk::trait]] FooIterator {
    Foo* operator->() const;
};

TEST(DuckViewTest, FooIterator) {
    std::vector<Foo> vec{Foo{10}, Foo{5}, Foo{-5}};
    rjk::duck_view<FooIterator> view{vec.begin()};
    ASSERT_EQ(view->data, 10);
    view = vec.end() - 1;
    ASSERT_EQ(view->data, -5);
}

struct [[=rjk::trait]] MyTrait {
    void doSmth();
    std::span<const int> getData() const;
};

int myFunction(rjk::duck_view<const MyTrait> withData, std::size_t index) {
    return withData.getData()[index];
}

TEST(DuckViewTest, FunctionArgument) {
    struct A {
    private:
        std::array<int, 10> data{1,2,3,4,5,6,7,8,9,10};
    public:
        std::span<const int> getData() const { return data; }

        void doSmth() {
            for (auto& num : data) { num++; }
        }
    };

    A a{};
    EXPECT_EQ(myFunction(a, 5), 6);
    a.doSmth();
    EXPECT_EQ(myFunction(a, 5), 7);

    struct B {
        std::span<const int> getData() const {
            static std::array data{5, 3, 2};
            return data;
        }

        void doSmth() {}
    };

    EXPECT_EQ(myFunction(B{}, 2), 2);
}

// ============================================================
// Trait definitions
// ============================================================

struct [[=rjk::trait]] Printable {
    std::string toString() const;
};

struct [[=rjk::trait]] Counter {
    void increment();
    void decrement();
    int value() const;
};

struct [[=rjk::trait]] Resizable {
    void resize(std::size_t n);
    std::size_t size() const;
};

// Uses std::reference_wrapper to work around GCC bug with ref-qualified returns
struct [[=rjk::trait]] RefAccessor {
    std::reference_wrapper<int> front();
    std::reference_wrapper<const int> front() const;
};

struct [[=rjk::trait]] Indexable {
    int operator[](std::size_t i) const;
};


// ============================================================
// Concrete types (no inheritance from any base)
// ============================================================

struct SimpleCounter {
    int val = 0;
    void increment() { ++val; }
    void decrement() { --val; }
    int value() const { return val; }
    std::string toString() const { return std::to_string(val); }
};

struct SaturatingCounter {
    int val = 0;
    int max;
    explicit SaturatingCounter(int max) : max(max) {}
    void increment() { if (val < max) ++val; }
    void decrement() { if (val > 0) --val; }
    int value() const { return val; }
    std::string toString() const { return std::to_string(val) + "/" + std::to_string(max); }
};

struct Buffer {
    std::vector<int> data;
    explicit Buffer(std::initializer_list<int> init) : data(init) {}
    void resize(std::size_t n) { data.resize(n, 0); }
    std::size_t size() const { return data.size(); }
    std::reference_wrapper<int> front() { return data.front(); }
    std::reference_wrapper<const int> front() const { return data.front(); }
    int operator[](std::size_t i) const { return data[i]; }
};

// ============================================================
// Basic duck_view: single trait, method calls
// ============================================================

TEST(DuckViewTest, SingleTraitMethodCalls) {
    SimpleCounter c{};
    rjk::duck_view<Counter> view{c};

    EXPECT_EQ(view.value(), 0);
    view.increment();
    view.increment();
    EXPECT_EQ(view.value(), 2);
    view.decrement();
    EXPECT_EQ(view.value(), 1);

    // Verify the original is mutated
    EXPECT_EQ(c.val, 1);
}

TEST(DuckViewTest, Reassignment) {
    SimpleCounter a{};
    SaturatingCounter b{3};

    rjk::duck_view<Counter> view{a};
    view.increment();
    EXPECT_EQ(view.value(), 1);

    view = b;
    EXPECT_EQ(view.value(), 0);
    view.increment();
    view.increment();
    view.increment();
    view.increment(); // saturates at 3
    EXPECT_EQ(view.value(), 3);
    EXPECT_EQ(b.val, 3);
}

// ============================================================
// const trait: only const methods exposed
// ============================================================

TEST(DuckViewTest, ConstTraitOnlyExposesConstMethods) {
    SimpleCounter c{.val = 5};
    rjk::duck_view<const Counter> view{c};

    EXPECT_EQ(view.value(), 5);

    // increment() and decrement() are non-const, so they must not compile.
}

TEST(DuckViewTest, ConstTraitFromConstObject) {
    const SimpleCounter c{.val = 7};
    rjk::duck_view<const Counter> view{c};
    EXPECT_EQ(view.value(), 7);
}

// ============================================================
// Multiple traits on one duck_view
// ============================================================

TEST(DuckViewTest, MultipleTraits) {
    SimpleCounter c{};
    rjk::duck_view<Counter, Printable> view{c};

    view.increment();
    view.increment();
    view.increment();
    EXPECT_EQ(view.value(), 3);
    EXPECT_EQ(view.toString(), "3");
}

TEST(DuckViewTest, MultipleTraitsReassignment) {
    SimpleCounter a{};
    SaturatingCounter b{2};

    rjk::duck_view<Counter, Printable> view{a};
    view.increment();
    EXPECT_EQ(view.toString(), "1");

    view = b;
    view.increment();
    view.increment();
    view.increment(); // saturates
    EXPECT_EQ(view.toString(), "2/2");
}

// ============================================================
// duck_view borrowing from an owning duck
// ============================================================

TEST(DuckViewTest, BorrowFromDuck) {
    rjk::duck<Counter> d{SimpleCounter{}};

    rjk::duck_view<Counter> view{d};
    view.increment();
    view.increment();

    EXPECT_EQ(d.value(), 2);
    EXPECT_EQ(view.value(), 2);
}

TEST(DuckViewTest, BorrowFromDuckMultipleTraits) {
    rjk::duck<Counter, Printable> d{SimpleCounter{}};
    d.increment();
    d.increment();

    rjk::duck_view<Counter, Printable> view{d};
    EXPECT_EQ(view.value(), 2);
    EXPECT_EQ(view.toString(), "2");

    view.increment();
    EXPECT_EQ(d.value(), 3);
}

// ============================================================
// Operators
// ============================================================

TEST(DuckViewTest, IndexOperator) {
    Buffer buf{{10, 20, 30, 40}};
    rjk::duck_view<Indexable> view{buf};

    EXPECT_EQ(view[0], 10);
    EXPECT_EQ(view[2], 30);
    EXPECT_EQ(view[3], 40);
}

// ============================================================
// ref-returning methods via std::reference_wrapper (GCC workaround)
// ============================================================

TEST(DuckViewTest, RefAccessorMutation) {
    Buffer buf{{1, 2, 3}};
    rjk::duck_view<RefAccessor> view{buf};

    view.front().get() = 99;
    EXPECT_EQ(buf.data[0], 99);
}

TEST(DuckViewTest, RefAccessorConst) {
    const Buffer buf{{5, 6, 7}};
    rjk::duck_view<const RefAccessor> view{buf};

    EXPECT_EQ(view.front().get(), 5);
}

// ============================================================
// Resize + RefAccessor together (multiple traits, includes operator)
// ============================================================

TEST(DuckViewTest, ResizableAndRefAccessor) {
    Buffer buf{{1}};
    rjk::duck_view<Resizable, RefAccessor> view{buf};

    EXPECT_EQ(view.size(), 1u);
    view.resize(5);
    EXPECT_EQ(view.size(), 5u);
    EXPECT_EQ(buf.data.size(), 5u);

    view.front().get() = 42;
    EXPECT_EQ(buf.data[0], 42);
}

// ============================================================
// get<T>() / get_if<T>() downcasting
// ============================================================

TEST(DuckViewTest, GetCorrectType) {
    SimpleCounter c{.val = 3};
    rjk::duck_view<Counter> view{c};

    EXPECT_NO_THROW({
        auto& ref = view.get<SimpleCounter>();
        EXPECT_EQ(ref.val, 3);
    });
}

TEST(DuckViewTest, GetWrongTypeThrows) {
    SimpleCounter c{};
    rjk::duck_view<Counter> view{c};

    EXPECT_THROW(view.get<SaturatingCounter>(), rjk::bad_duck_access);
}

TEST(DuckViewTest, GetIfCorrectType) {
    SimpleCounter c{.val = 7};
    rjk::duck_view<Counter> view{c};

    auto* ptr = view.get_if<SimpleCounter>();
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(ptr->val, 7);
}

TEST(DuckViewTest, GetIfWrongTypeReturnsNull) {
    SimpleCounter c{};
    rjk::duck_view<Counter> view{c};

    auto* ptr = view.get_if<SaturatingCounter>();
    EXPECT_EQ(ptr, nullptr);
}

TEST(DuckViewTest, GetIfConstView) {
    SimpleCounter c{.val = 9};
    const rjk::duck_view<Counter> view{c};

    const SimpleCounter* ptr = view.get_if<SimpleCounter>();
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(ptr->val, 9);
}

// ============================================================
// duck_view in a heterogeneous container (via duck)
// ============================================================

TEST(DuckViewTest, HeterogeneousViewLoop) {
    SimpleCounter a{.val = 0};
    SaturatingCounter b{5};

    std::vector<rjk::duck_view<Counter>> views{};
    views.emplace_back(a);
    views.emplace_back(b);

    for (auto& v : views) {
        v.increment();
        v.increment();
    }

    EXPECT_EQ(a.val, 2);
    EXPECT_EQ(b.val, 2);

    int total = std::accumulate(views.begin(), views.end(), 0,
        [](int acc, auto& v) { return acc + v.value(); });
    EXPECT_EQ(total, 4);
}

// ============================================================
// duck_view lifetime: rebinding doesn't affect previous target
// ============================================================

TEST(DuckViewTest, RebindDoesNotAffectPrevious) {
    SimpleCounter a{.val = 10};
    SimpleCounter b{.val = 20};

    rjk::duck_view<Counter> view{a};
    view.increment(); // a = 11

    view = b;
    view.increment(); // b = 21

    EXPECT_EQ(a.val, 11);
    EXPECT_EQ(b.val, 21);
}

// ============================================================
// Passing duck_view to functions (matching the FunctionArgument pattern)
// ============================================================

static int sumFirstN(rjk::duck_view<const Resizable> r, rjk::duck_view<const Indexable> idx, std::size_t n) {
    int total = 0;
    for (std::size_t i = 0; i < std::min(n, r.size()); ++i) {
        total += idx[i];
    }
    return total;
}

TEST(DuckViewTest, SeparateTraitViewsAsFunctionArgs) {
    Buffer buf{{1, 2, 3, 4, 5}};
    EXPECT_EQ(sumFirstN(buf, buf, 3), 6);
    EXPECT_EQ(sumFirstN(buf, buf, 5), 15);
}

struct [[=rjk::trait]] TraitA {
    void setData(int data);
    int getData() const;
};

struct [[=rjk::trait]] TraitB {
    void doSmth();
    int getSmth() const;
};

bool readData(rjk::duck_view<const TraitA, const TraitB> view) {
    return view.getSmth() > view.getData();
}

TEST(DuckViewTest, ConstDuckView) {
    struct TestStruct {
        int d = 0;

        void setData(int data) { d = data; }
        int getData() const { return d; }

        void doSmth() { d *= 2; }
        int getSmth() const { return d * 2; }
    };

    rjk::duck<TraitA, TraitB> b{TestStruct{}};
    b.setData(10);
    EXPECT_EQ(b.getData(), 10);
    b.doSmth();
    EXPECT_TRUE(readData(b));
}

}