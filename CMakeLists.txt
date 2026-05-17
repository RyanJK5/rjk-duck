cmake_minimum_required(VERSION 4.2)
project(RJK_Duck VERSION 0.1.0 LANGUAGES CXX)

set(PROJECT_VERSION_SUFFIX "-alpha.1")

message(STATUS "Building version ${PROJECT_VERSION}${PROJECT_VERSION_SUFFIX}")

include(CTest)
include(FetchContent)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CXX_CLANG_TIDY "clang-tidy")

add_compile_options(-Wall -Wextra -Wpedantic)
set(CMAKE_COMPILE_WARNING_AS_ERROR ON)

FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.16.0.zip
        SYSTEM
)
FetchContent_MakeAvailable(googletest)

add_library(RJK_Duck INTERFACE
        include/duck_tags.hpp)

target_include_directories(RJK_Duck INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_subdirectory(tests)