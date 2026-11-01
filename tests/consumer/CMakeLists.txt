# A project that uses an installed DocSmith++, as a consumer would. run.cmake
# installs the library and builds and runs this against it.
cmake_minimum_required(VERSION 3.20)

project(DocSmithCppConsumer LANGUAGES CXX)

find_package(DocSmithCpp CONFIG REQUIRED)

add_executable(consumer main.cpp)
target_link_libraries(consumer PRIVATE docsmith::docsmithcpp)

enable_testing()
add_test(NAME consumer COMMAND consumer WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
