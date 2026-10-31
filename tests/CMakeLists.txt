include(GoogleTest)

add_executable(docsmithcpp_tests test_main.cpp "odt/test_odt.cpp" "basic_usage.cpp")
target_link_libraries(docsmithcpp_tests PRIVATE docsmithcpp GTest::gtest fmt::fmt)

# Fixture paths in the tests are relative to this directory; main() changes into
# it so the executable can be run from anywhere, not just via ctest.
target_compile_definitions(docsmithcpp_tests PRIVATE
    DOCSMITHCPP_TEST_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

gtest_discover_tests(docsmithcpp_tests WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
