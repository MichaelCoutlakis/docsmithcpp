include(GoogleTest)

add_executable(docsmithcpp_tests
    test_main.cpp
    basic_usage.cpp
    odt/test_odt.cpp
    odt/test_odt_xml.cpp
    odt/test_package.cpp
    zip/test_zip.cpp
    html/test_html.cpp)
target_link_libraries(docsmithcpp_tests PRIVATE docsmithcpp GTest::gtest pugixml::pugixml)

# White-box tests (odt/test_odt_xml.cpp, the zip and package tests) read the
# private headers.
target_include_directories(docsmithcpp_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)

# Fixture paths in the tests are relative to this directory; main() changes into
# it so the executable can be run from anywhere, not just via ctest.
target_compile_definitions(docsmithcpp_tests PRIVATE
    DOCSMITHCPP_TEST_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

gtest_discover_tests(docsmithcpp_tests WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

# Install the library and build a separate project against it, the way a
# consumer would with find_package. Configures and builds from scratch, so it
# takes a while; skip it with ctest -LE consumer.
set(consumer_forward
    CMAKE_TOOLCHAIN_FILE
    VCPKG_TARGET_TRIPLET
    VCPKG_INSTALLED_DIR
    CMAKE_C_COMPILER
    CMAKE_CXX_COMPILER
    CMAKE_EXE_LINKER_FLAGS
    CMAKE_MAKE_PROGRAM)
set(consumer_args)
foreach(var IN LISTS consumer_forward)
    list(APPEND consumer_args "-D${var}=${${var}}")
endforeach()
# The build type is the configuration under test. Comma-separated, since a ;
# would split the argument.
list(APPEND consumer_forward CMAKE_BUILD_TYPE)
list(JOIN consumer_forward "," consumer_forward_arg)

add_test(NAME consumer
    COMMAND ${CMAKE_COMMAND}
        "-DLIBRARY_BINARY_DIR=${PROJECT_BINARY_DIR}"
        "-DPREFIX=${CMAKE_CURRENT_BINARY_DIR}/consumer/prefix"
        "-DCONSUMER_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/consumer"
        "-DCONSUMER_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}/consumer/build"
        "-DCONFIG=$<CONFIG>"
        "-DGENERATOR=${CMAKE_GENERATOR}"
        "-DPLATFORM=${CMAKE_GENERATOR_PLATFORM}"
        "-DCTEST_COMMAND=${CMAKE_CTEST_COMMAND}"
        "-DFORWARD=${consumer_forward_arg}"
        ${consumer_args}
        "-DCMAKE_BUILD_TYPE=$<CONFIG>"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/consumer/run.cmake)
set_tests_properties(consumer PROPERTIES LABELS consumer TIMEOUT 900)
