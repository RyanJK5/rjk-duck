cmake_minimum_required(VERSION 3.25)
project(rjk_duck VERSION 0.1.0 LANGUAGES CXX)

include(CTest)

add_library(duck INTERFACE)
add_library(rjk::duck ALIAS duck)

target_include_directories(duck INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_sources(duck INTERFACE
    FILE_SET HEADERS
    BASE_DIRS include
    FILES include/rjk/duck.hpp
)
target_compile_options(duck INTERFACE -std=c++26 -freflection)

if(PROJECT_IS_TOP_LEVEL)
    option(RJK_DUCK_BUILD_TESTS "Build tests" OFF)
    option(RJK_DUCK_BUILD_BENCHMARKS "Build benchmarks" OFF)

    if(RJK_DUCK_BUILD_TESTS)
        add_subdirectory(tests)
    endif()

    if(RJK_DUCK_BUILD_BENCHMARKS)
        add_subdirectory(benchmark)
    endif()

    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)

    install(TARGETS duck
            EXPORT rjk_duck_targets
            FILE_SET HEADERS
    )

    install(EXPORT rjk_duck_targets
            FILE rjk_duck-targets.cmake
            NAMESPACE rjk::
            DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/rjk_duck
    )

    configure_package_config_file(
            ${CMAKE_CURRENT_SOURCE_DIR}/cmake/rjk_duck-config.cmake.in
            ${CMAKE_CURRENT_BINARY_DIR}/rjk_duck-config.cmake
            INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/rjk_duck
    )

    write_basic_package_version_file(
            ${CMAKE_CURRENT_BINARY_DIR}/rjk_duck-config-version.cmake
            VERSION ${PROJECT_VERSION}
            COMPATIBILITY SameMajorVersion
    )

    install(FILES
            ${CMAKE_CURRENT_BINARY_DIR}/rjk_duck-config.cmake
            ${CMAKE_CURRENT_BINARY_DIR}/rjk_duck-config-version.cmake
            DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/rjk_duck
    )
endif()