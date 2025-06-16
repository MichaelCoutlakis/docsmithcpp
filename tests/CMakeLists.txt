add_executable(docsmithcpp_tests test_main.cpp "odt/test_odt.cpp")
target_link_libraries(docsmithcpp_tests PRIVATE docsmithcpp GTest::GTest)
add_test(NAME all_tests COMMAND docsmithcpp_tests)


    gtest_discover_tests(docsmithcpp_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/)