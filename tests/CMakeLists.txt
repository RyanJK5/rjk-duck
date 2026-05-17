add_executable(RJK_Any_Tests test.cpp)
target_link_libraries(RJK_Any_Tests PRIVATE RJK_Any GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(RJK_Any_Tests)