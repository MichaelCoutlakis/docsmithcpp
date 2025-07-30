cmake_minimum_required(VERSION 3.20)
project(DocSmithCpp VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")

# Options
option(BUILD_SHARED_LIBS "Build shared libraries" OFF)
option(DOCSMITHCPP_BUILD_TESTS "Build unit tests" OFF)
option(DOCSMITHCPP_BUILD_MINIMAL_USAGE "Build only minimual main demonstrating usage" ON)

# Output directories
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)


# Example external deps
find_package(pugixml CONFIG REQUIRED)
find_package(libzippp CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)

# Add subdirectories
add_subdirectory(src)
if(DOCSMITHCPP_BUILD_TESTS)
    find_package(GTest REQUIRED)
    enable_testing()
    add_subdirectory(tests)
endif()

# Install config support
include(CMakePackageConfigHelpers)
install(EXPORT DocSmithCppTargets
        FILE DocSmithCppTargets.cmake
        NAMESPACE docsmith::
        DESTINATION lib/cmake/DocSmithCpp)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/DocSmithCppConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/DocSmithCppConfig.cmake"
    INSTALL_DESTINATION lib/cmake/DocSmithCpp
)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/DocSmithCppConfig.cmake"
        DESTINATION lib/cmake/DocSmithCpp)