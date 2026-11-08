function(docsmith_example name)
    add_executable(example_${name} ${name}.cpp)
    target_link_libraries(example_${name} PRIVATE docsmith::docsmithcpp)
    # Where report_template.odt is.
    target_compile_definitions(example_${name} PRIVATE
        DOCSMITHCPP_EXAMPLES_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
endfunction()

docsmith_example(create_document)
docsmith_example(read_odt)
docsmith_example(odt_to_html)
docsmith_example(docx_to_html)
docsmith_example(make_report_template)
docsmith_example(fill_template)
docsmith_example(md)
if(DOCSMITHCPP_MARKDOWN)
    docsmith_example(showcase)
    # Only a showcase that compiles reaches the README.
    if(DOCSMITHCPP_SYNC_README)
        add_custom_command(TARGET example_showcase POST_BUILD
            COMMAND ${CMAKE_COMMAND} "-DSOURCE_DIR=${PROJECT_SOURCE_DIR}"
                -P ${PROJECT_SOURCE_DIR}/cmake/SyncReadmeShowcase.cmake
            VERBATIM)
    endif()
    if(DOCSMITHCPP_BUILD_TESTS)
        add_test(NAME example_showcase COMMAND example_showcase
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
    endif()
endif()
