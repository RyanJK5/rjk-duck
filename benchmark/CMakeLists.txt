include(FetchContent)
include("${CMAKE_SOURCE_DIR}/cmake/CPM.cmake")

# Google Benchmark
FetchContent_Declare(
    googlebenchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG        v1.9.1
    SYSTEM
)
set(BENCHMARK_ENABLE_TESTING      OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_GTEST_TESTS  OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_INSTALL      OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googlebenchmark)

# AnyAny
FetchContent_Declare(
    anyany
    GIT_REPOSITORY https://github.com/kelbon/AnyAny
    GIT_TAG        v1.2.1
    SYSTEM
)
FetchContent_MakeAvailable(anyany)

# Proxy
CPMAddPackage(
    NAME msft_proxy4
    GIT_TAG 4.1.0
    GIT_REPOSITORY https://github.com/ngcpp/proxy.git
)

function (add_benchmark name)
    set(target_name "rjk_duck_bench_${name}")
    add_executable(${target_name} ${ARGN})
    add_executable(rjk::duck::bench_${name} ALIAS ${target_name})

    target_include_directories(${target_name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_compile_options(${target_name} PRIVATE -fno-lto)
    target_link_libraries(${target_name} PRIVATE
        rjk::duck
        benchmark::benchmark_main
        anyanylib
        msft_proxy4::proxy
    )
endfunction()

add_benchmark(dispatch
    dispatch/benchmark.cpp
    dispatch/factories.cpp
)

add_benchmark(lifetime
    lifetime/benchmark.cpp
)

add_custom_target(rjk_duck_benchmark
    COMMAND $<TARGET_FILE:rjk_duck_bench_dispatch>
    COMMAND $<TARGET_FILE:rjk_duck_bench_lifetime>
    DEPENDS rjk_duck_bench_dispatch rjk_duck_bench_lifetime
)