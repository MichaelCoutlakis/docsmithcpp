cmake_minimum_required(VERSION 3.20)

project(DocSmithCpp VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

message("CMAKE_PREFIX_PATH: ${CMAKE_PREFIX_PATH}")
message("CMAKE_TOOLCHAIN_FILE: ${CMAKE_TOOLCHAIN_FILE}")

add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")

# Tests and examples default on only when this is the top-level project, so
# consumers using add_subdirectory / FetchContent don't build them.
if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    set(docsmithcpp_is_top_level ON)
else()
    set(docsmithcpp_is_top_level OFF)
endif()

option(BUILD_SHARED_LIBS "Build shared libraries" OFF)
option(DOCSMITHCPP_BUILD_TESTS "Build unit tests" ${docsmithcpp_is_top_level})
option(DOCSMITHCPP_BUILD_EXAMPLES "Build examples" ${docsmithcpp_is_top_level})
option(DOCSMITHCPP_DEBUG_FILES "Write debug dumps (e.g. <name>.content.xml next to saved .odt files)" OFF)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

find_package(pugixml CONFIG REQUIRED)
find_package(libzippp CONFIG REQUIRED)

add_subdirectory(src)

if(DOCSMITHCPP_BUILD_TESTS)
    find_package(GTest REQUIRED)
    enable_testing()
    add_subdirectory(tests)
endif()

if(DOCSMITHCPP_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Install / package config
include(CMakePackageConfigHelpers)
install(EXPORT DocSmithCppTargets
        FILE DocSmithCppTargets.cmake
        NAMESPACE docsmith::
        DESTINATION lib/cmake/DocSmithCpp)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/DocSmithCppConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/DocSmithCppConfig.cmake"
    INSTALL_DESTINATION lib/cmake/DocSmithCpp)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/DocSmithCppConfigVersion.cmake"
    COMPATIBILITY SameMinorVersion)

install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/DocSmithCppConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/DocSmithCppConfigVersion.cmake"
        DESTINATION lib/cmake/DocSmithCpp)
