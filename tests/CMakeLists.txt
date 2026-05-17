add_executable(RJK_Duck_Tests test.cpp)
target_link_libraries(RJK_Duck_Tests PRIVATE RJK_Duck GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(RJK_Duck_Tests)