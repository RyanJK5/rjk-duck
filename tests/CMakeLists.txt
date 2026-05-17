add_executable(RJK_Any_Tests test.cpp)

include(GoogleTest)
gtest_discover_tests(RJK_Any_Tests)