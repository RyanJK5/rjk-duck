include(FetchContent)
include(CTest)

FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.16.0.zip
    SYSTEM
)
FetchContent_MakeAvailable(googletest)

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")

add_executable(rjk_duck_tests test_operators.cpp
    test_fixtures.hpp
    test_construction.cpp
    test_copy_move.cpp
        test_lifetime.cpp
    test_storage.cpp
    test_stdlib.cpp
    test_overloads.cpp
    test_member_operators.cpp
    test_variadic_operators.cpp
    test_multiple_traits.cpp
    test_duck_view.cpp
    test_subsumption.cpp
    test_return_deduction.cpp
    test_like.cpp
    test_impl.cpp
    test_constexpr.cpp
        test_access.cpp
)

target_link_libraries(rjk_duck_tests PRIVATE rjk::duck GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(rjk_duck_tests)