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
docsmith_example(make_report_template)
docsmith_example(fill_template)
docsmith_example(md)
