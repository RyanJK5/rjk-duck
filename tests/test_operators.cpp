// #include "duck.hpp"
//
// #include <gtest/gtest.h>
//
// namespace rjk_test {
// TEST(BasicOperator, Plus) {
//     using BasicPlus = rjk::duck<
//         rjk::has_op<rjk::op_plus, int(rjk::self, int)>,
//         rjk::has_op<rjk::op_plus, int(int, rjk::self)>
//     >;
//
//     BasicPlus x{10};
//
//     EXPECT_EQ(x + 5, 15);
//     EXPECT_EQ(5 + x + 5, 20);
// }
//
// using CommutativePlus = rjk::duck<
//     rjk::has_op<rjk::op_plus, int(rjk::self&, const rjk::self&)>,
//     rjk::has_op<rjk::op_plus, int(const rjk::self&, rjk::self&)>
// >;
//
// struct CommutativePlusA {
//     int operator+(const CommutativePlus&) {
//         return 5;
//     }
// };
//
// int operator+(const CommutativePlus&, CommutativePlusA&) {
//     return 25;
// }
//
// TEST(BasicOperator, PlusOtherCommutative) {
//     CommutativePlus x{CommutativePlusA{}};
//     CommutativePlus y{CommutativePlusA{}};
//
//     EXPECT_EQ(x + y, 5);
//     EXPECT_EQ(y + x, 5);
//     EXPECT_EQ(x + y, 25);
//     EXPECT_EQ(y + x, 25);
// }
//
// TEST(BasicOperator, PlusOther) {
//     using BasicPlus = rjk::duck<
//         rjk::has_op<rjk::op_plus, int(rjk::self&, const rjk::self&)>
//     >;
//
//     struct A {
//         int operator+(const BasicPlus&) {
//             return 5;
//         }
//     };
//
//     struct B {
//         int operator+(const BasicPlus&) {
//             return 6;
//         }
//     };
//
//     BasicPlus a{A{}};
//     BasicPlus b{B{}};
//
//     EXPECT_EQ(a + b, 5);
//     EXPECT_EQ(b + a, 6);
// }
//
// TEST(BasicOperator, RefTest) {
//     using RefPlus = rjk::duck<
//         rjk::has_op<rjk::op_plus, int(rjk::self &, rjk::self &)>
//     >;
//
//     struct A {
//         int operator+(RefPlus&) const {
//             return 10;
//         }
//
//         int operator+(RefPlus&) {
//             return 20;
//         }
//     };
//
//     RefPlus x{A{}};
//     RefPlus y{A{}};
//     EXPECT_EQ(x + y, 20);
// }
//
// TEST(BasicOperator, ConstRefTest) {
//     using ConstRefPlus = rjk::duck<
//         rjk::has_op<rjk::op_plus, int(const rjk::self &, const rjk::self &)>
//     >;
//
//     struct A {
//         int operator+(const ConstRefPlus&) {
//             return 10;
//         }
//
//         int operator+(const ConstRefPlus&) const {
//             return 20;
//         }
//     };
//
//     ConstRefPlus x{A{}};
//     ConstRefPlus y{A{}};
//     EXPECT_EQ(x + y, 20);
// }
//
// TEST(BasicOperator, RValueTest) {
//     using RValuePlus = rjk::duck<
//         rjk::has_op<rjk::op_plus, int(rjk::self &&, const rjk::self &)>
//     >;
//
//     struct A {
//         int operator+(const RValuePlus&) && {
//             return 10;
//         }
//
//         int operator+(const RValuePlus&) const & {
//             return 20;
//         }
//     };
//
//     RValuePlus x{A{}};
//     RValuePlus y{A{}};
//     EXPECT_EQ(RValuePlus{x} + std::move(y), 10);
// }
//
// TEST(BasicOperator, MultipleOverloads) {
//     using Test = rjk::duck<
//         rjk::has_op<rjk::op_plus, int(rjk::self, const rjk::self&)>
//     >;
//
//     struct A {
//         int operator+(const Test&) {
//             return 5;
//         }
//
//         int operator+(int) {
//             return 10;
//         }
//     };
//
//     Test x{A{}};
//     Test y{A{}};
//     EXPECT_EQ(x + Test{A{}}, 5);
// }
// }
