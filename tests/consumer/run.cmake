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
# Only CONFIG is installed. A multi-config generator would also generate the
# others, and fail when vcpkg's mappings (e.g. MinSizeRel to Release) find no
# library for them.
if(GENERATOR MATCHES "^Visual Studio|^Xcode|Multi-Config$")
    list(APPEND args "-DCMAKE_CONFIGURATION_TYPES=${CONFIG}")
endif()
string(REPLACE "," ";" FORWARD "${FORWARD}")
foreach(var IN LISTS FORWARD)
    if(DEFINED ${var} AND NOT "${${var}}" STREQUAL "")
        list(APPEND args "-D${var}=${${var}}")
    endif()
endforeach()

list(APPEND args "-DWITH_MARKDOWN=${WITH_MARKDOWN}")

# Without its dependencies found first, DocSmith++ refuses to load, naming
# them.
execute_process(
    COMMAND "${CMAKE_COMMAND}" -S "${CONSUMER_SOURCE_DIR}" -B "${CONSUMER_BINARY_DIR}-bare"
        ${args} -DFIND_DEPENDENCIES=OFF
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE output)
file(REMOVE_RECURSE "${CONSUMER_BINARY_DIR}-bare")
if(result EQUAL 0)
    message(FATAL_ERROR "find_package(DocSmithCpp) succeeded without its dependencies")
endif()
# CMake wraps the message, so only look for each piece in it.
foreach(piece IN ITEMS "Missing:" pugixml::pugixml miniz::miniz)
    string(FIND "${output}" "${piece}" at)
    if(at EQUAL -1)
        message(FATAL_ERROR "The failure doesn't say \"${piece}\":\n${output}")
    endif()
endforeach()

run("${CMAKE_COMMAND}" -S "${CONSUMER_SOURCE_DIR}" -B "${CONSUMER_BINARY_DIR}" ${args})
run("${CMAKE_COMMAND}" --build "${CONSUMER_BINARY_DIR}" --config "${CONFIG}")
run("${CTEST_COMMAND}" --test-dir "${CONSUMER_BINARY_DIR}" -C "${CONFIG}" --output-on-failure --no-tests=error)
