cmake_minimum_required(VERSION 3.25)
project(RJK_Duck VERSION 0.1.0 LANGUAGES CXX)

set(PROJECT_VERSION_SUFFIX "-alpha.1")

message(STATUS "Building version ${PROJECT_VERSION}${PROJECT_VERSION_SUFFIX}")

include(CTest)
include(FetchContent)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-Wall -Wextra -Wpedantic -std=c++26 -freflection -lstdc++exp)
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
set(CMAKE_COMPILE_WARNING_AS_ERROR ON)

FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.16.0.zip
    SYSTEM
)
FetchContent_MakeAvailable(googletest)

add_library(RJK_Duck INTERFACE
    src/duck.hpp
    src/duck_tags.hpp
    src/detail/storage.hpp
    src/detail/flag_enum.hpp
    src/detail/vtable_fn_maker.hpp
    src/detail/duck_base.hpp
    src/duck_view.hpp
    src/duck_view.hpp
    src/detail/duck_behavior_base.hpp
    src/detail/vtable_generator.hpp
    src/detail/display_error.hpp
    src/detail/subsumption_utils.hpp
    src/detail/meta_util.hpp
)

target_include_directories(RJK_Duck INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_subdirectory(tests)