cmake_minimum_required(VERSION 3.25)
project(rjk_duck VERSION 0.1.0 LANGUAGES CXX)

option(RJK_DUCK_MODULE "Build rjk::duck as a C++ module" OFF)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_EXTENSIONS OFF)

if (RJK_DUCK_MODULE)
    add_library(rjk_duck)

    target_include_directories(rjk_duck PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    )

    target_sources(rjk_duck PUBLIC
        FILE_SET CXX_MODULES
        BASE_DIRS module
        FILES module/duck.cppm
    )

    target_sources(rjk_duck PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES include/rjk/duck.hpp
    )

    target_compile_features(rjk_duck PUBLIC cxx_std_23)
    target_compile_options(rjk_duck PUBLIC -std=c++26 -freflection)
else()
    add_library(rjk_duck INTERFACE)

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
endif()

add_library(rjk::duck ALIAS rjk_duck)
set_target_properties(rjk_duck PROPERTIES EXPORT_NAME duck)

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

    if(RJK_DUCK_MODULE)
        install(TARGETS rjk_duck
            EXPORT rjk_duck_targets
            FILE_SET CXX_MODULES DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/rjk_duck/modules
            FILE_SET HEADERS
        )

        install(EXPORT rjk_duck_targets
            FILE rjk_duck-targets.cmake
            NAMESPACE rjk::
            DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/rjk_duck
            CXX_MODULES_DIRECTORY cxx_modules
        )
    else()
        install(TARGETS rjk_duck
            EXPORT rjk_duck_targets
            FILE_SET HEADERS
        )

        install(EXPORT rjk_duck_targets
            FILE rjk_duck-targets.cmake
            NAMESPACE rjk::
            DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/rjk_duck
        )
    endif()

    configure_package_config_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/rjk_duck-config.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/rjk_duck-config.cmake
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/rjk_duck
    )

    write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/rjk_duck-config-version.cmake
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
        ARCH_INDEPENDENT
    )

    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/rjk_duck-config.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/rjk_duck-config-version.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/rjk_duck
    )
endif()