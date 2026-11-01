# Installs the library into a prefix, then configures, builds and runs the
# consumer project against it with the same toolchain. Run by ctest; see
# tests/CMakeLists.txt for the variables it is given.

file(REMOVE_RECURSE "${PREFIX}" "${CONSUMER_BINARY_DIR}")

function(run)
    execute_process(COMMAND ${ARGN} RESULT_VARIABLE result)
    if(NOT result EQUAL 0)
        list(JOIN ARGN " " command)
        message(FATAL_ERROR "Failed (${result}): ${command}")
    endif()
endfunction()

run("${CMAKE_COMMAND}" --install "${LIBRARY_BINARY_DIR}" --config "${CONFIG}" --prefix "${PREFIX}")

set(args -G "${GENERATOR}" "-DCMAKE_PREFIX_PATH=${PREFIX}")
if(PLATFORM)
    list(APPEND args -A "${PLATFORM}")
endif()
string(REPLACE "," ";" FORWARD "${FORWARD}")
foreach(var IN LISTS FORWARD)
    if(DEFINED ${var} AND NOT "${${var}}" STREQUAL "")
        list(APPEND args "-D${var}=${${var}}")
    endif()
endforeach()

run("${CMAKE_COMMAND}" -S "${CONSUMER_SOURCE_DIR}" -B "${CONSUMER_BINARY_DIR}" ${args})
run("${CMAKE_COMMAND}" --build "${CONSUMER_BINARY_DIR}" --config "${CONFIG}")
run("${CTEST_COMMAND}" --test-dir "${CONSUMER_BINARY_DIR}" -C "${CONFIG}" --output-on-failure --no-tests=error)
