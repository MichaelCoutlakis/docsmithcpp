# A project that uses an installed DocSmith++, as a consumer would. run.cmake
# installs the library and builds and runs this against it.
cmake_minimum_required(VERSION 3.20)

project(DocSmithCppConsumer LANGUAGES CXX)

# The consumer finds what DocSmith++ links: it decides which libraries are in
# its build. FIND_DEPENDENCIES OFF checks that DocSmith++ then refuses to load.
option(FIND_DEPENDENCIES "Find DocSmith++'s dependencies first" ON)
option(WITH_MARKDOWN "DocSmith++ was built with DOCSMITHCPP_MARKDOWN" OFF)
if(FIND_DEPENDENCIES)
    find_package(pugixml CONFIG REQUIRED)
    find_package(miniz CONFIG REQUIRED)
    if(WITH_MARKDOWN)
        find_package(md4c CONFIG REQUIRED)
    endif()
endif()

find_package(DocSmithCpp CONFIG REQUIRED)

add_executable(consumer main.cpp)
target_link_libraries(consumer PRIVATE docsmith::docsmithcpp)

enable_testing()
add_test(NAME consumer COMMAND consumer WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
