cmake_minimum_required(VERSION 3.25)
project(RJK_Duck VERSION 0.1.0 LANGUAGES CXX)

include(CTest)

add_library(rjk_duck INTERFACE)
add_library(rjk::duck ALIAS rjk_duck)

target_include_directories(rjk_duck INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_sources(rjk_duck INTERFACE
    FILE_SET HEADERS
    BASE_DIRS include
    FILES include/rjk/duck.hpp
)
target_compile_options(rjk_duck INTERFACE -std=c++26 -freflection)

if(PROJECT_IS_TOP_LEVEL)
    option(RJK_DUCK_BUILD_TESTS "Build tests" OFF)
    option(RJK_DUCK_BUILD_BENCHMARKS "Build benchmarks" OFF)

    if(RJK_DUCK_BUILD_TESTS)
        add_subdirectory(tests)
    endif()

    if(RJK_DUCK_BUILD_BENCHMARKS)
        add_subdirectory(benchmark)
    endif()
endif()