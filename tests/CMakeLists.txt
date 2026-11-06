include(FetchContent)
include(CTest)

FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.16.0.zip
    SYSTEM
)
# googletest is only a test dependency; keep it out of `cmake --install`.
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(rjk_duck_tests test_operators.cpp
    test_construction.cpp
    test_copy_move.cpp
    test_lifetime.cpp
    test_stdlib.cpp
    test_overloads.cpp
    test_variadic_operators.cpp
    test_multiple_traits.cpp
    test_duck_view.cpp
    test_subsumption.cpp
    test_return_deduction.cpp
    test_like.cpp
    test_impl.cpp
    test_access.cpp
    test_rejection.cpp
    test_constexpr.cpp
    allocators/basic.cpp
    allocators/extended_constructor.cpp
    allocators/propagation.cpp
    allocators/pmr.cpp
        allocators/conversion.cpp
        allocators/swap.cpp
        allocators/select_on_copy.cpp
        allocators/valueless.cpp
        allocators/exception_safety.cpp
)

target_compile_options(rjk_duck_tests PRIVATE
    -Werror -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wno-error=maybe-uninitialized)
target_link_libraries(rjk_duck_tests PRIVATE rjk::duck GTest::gtest_main)

if(RJK_DUCK_SANITIZE)
    # The null, alignment and vptr checks are disabled because GCC trunk
    # (seen with 17.0.0 20261025) rejects consteval calls in instrumented
    # code ("call to consteval function ... is not a constant expression",
    # then an ICE in cp_gimplify_expr) in tests/test_duck_view.cpp. Each of
    # the three triggers it on its own. nonnull-attribute and
    # returns-nonnull-attribute are disabled for the same family of bug: they
    # make vtable address comparisons in tests/test_constexpr.cpp "not a
    # constant expression" (seen with 20261004 and 20261101). See #28.
    set(RJK_DUCK_SANITIZER_FLAGS
        -fsanitize=address,undefined
        -fno-sanitize=null,alignment,vptr,nonnull-attribute,returns-nonnull-attribute
        -fno-sanitize-recover=undefined
        -fno-omit-frame-pointer)
    target_compile_options(rjk_duck_tests PRIVATE ${RJK_DUCK_SANITIZER_FLAGS})
    target_link_options(rjk_duck_tests PRIVATE ${RJK_DUCK_SANITIZER_FLAGS})
endif()

include(GoogleTest)
gtest_discover_tests(rjk_duck_tests)
