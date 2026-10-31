function(docsmith_example name)
    add_executable(example_${name} ${name}.cpp)
    target_link_libraries(example_${name} PRIVATE docsmith::docsmithcpp)
endfunction()

docsmith_example(create_basic_odt)
docsmith_example(read_odt)
