cmake_minimum_required(VERSION 3.25)
project(rjk_duck_install_test LANGUAGES CXX)

# Verifies that an installed rjk_duck package can be consumed via find_package.
# Configure with -DCMAKE_PREFIX_PATH=<install prefix>.
find_package(rjk_duck CONFIG REQUIRED)
message(STATUS "Using rjk_duck ${rjk_duck_VERSION} from ${rjk_duck_DIR}")

add_executable(install_test main.cpp)
target_link_libraries(install_test PRIVATE rjk::duck)

enable_testing()
add_test(NAME install_test COMMAND install_test)
