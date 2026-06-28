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

add_library(rjk_duck INTERFACE
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
        src/detail/vtable_caller.hpp
)

add_library(rjk::duck ALIAS rjk_duck)

target_include_directories(rjk_duck INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_subdirectory(benchmark)
add_subdirectory(tests)