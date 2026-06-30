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

add_executable(rjk_duck_benchmark
    basic_dispatch/benchmark.cpp
    basic_dispatch/factories.cpp
)

target_include_directories(rjk_duck_benchmark PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_options(rjk_duck_benchmark PRIVATE -fno-lto)

target_link_libraries(rjk_duck_benchmark PRIVATE
    rjk::duck
    benchmark::benchmark_main
)
