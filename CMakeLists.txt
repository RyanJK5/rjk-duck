cmake_minimum_required(VERSION 4.2)
project(RJK_Any VERSION 0.1.0 LANGUAGES CXX)

set(PROJECT_VERSION_SUFFIX "-alpha.1")

message(STATUS "Building version ${PROJECT_VERSION}${PROJECT_VERSION_SUFFIX}")

include(CTest)
include(FetchContent)

set(CMAKE_CXX_STANDARD 26)

FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.16.0.zip
        SYSTEM
)
FetchContent_MakeAvailable(googletest)

add_library(RJK_Any INTERFACE
        include/detail/bind_type_trait.hpp
        include/detail/fixed_string.hpp
        include/detail/fn_traits.hpp
        include/detail/remove_fn_qualifiers.hpp
        include/detail/remove_noexcept.hpp
        include/detail/substitute_fn_traits.hpp
        include/any.hpp
        include/any_tags.hpp)

add_subdirectory(tests)