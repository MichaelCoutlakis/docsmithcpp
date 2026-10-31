
function(docsmith_example name)
    add_executable(example_${name} ${name}.cpp)
    target_link_libraries(example_${name} PRIVATE docsmithcpp)
endfunction()

docsmith_example(create_basic_odt)
