cmake_minimum_required(VERSION 3.25)
project(RJK_Duck VERSION 0.1.0 LANGUAGES CXX)

include(CTest)
include(FetchContent)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-Wall -Wextra -std=c++26 -freflection -lstdc++exp)

FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.16.0.zip
    SYSTEM
)
FetchContent_MakeAvailable(googletest)

add_library(rjk_duck INTERFACE include/rjk/duck.hpp)
add_library(rjk::duck ALIAS rjk_duck)

add_subdirectory(benchmark)
add_subdirectory(tests)