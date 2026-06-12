add_executable(RJK_Duck_Tests test_operators.cpp
        test_fixtures.hpp
        test_construction.cpp
        test_copy_move.cpp
        test_access.cpp
        test_lifetime.cpp
        test_storage.cpp
        test_stdlib.cpp
        test_overloads.cpp
        test_member_operators.cpp
        test_variadic_operators.cpp
        test_multiple_traits.cpp
        test_duck_view.cpp
        test_subsumption.cpp
    )
target_link_libraries(RJK_Duck_Tests PRIVATE RJK_Duck GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(RJK_Duck_Tests)