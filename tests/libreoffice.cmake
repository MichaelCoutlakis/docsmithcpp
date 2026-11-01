# Runs the tests, then has LibreOffice convert every file they wrote to
# tests/odt/out and tests/docx/out to text. A file LibreOffice can't open gives
# no output, which fails the test.
#
# cmake -DSOFFICE=<soffice> -DTESTS=<test executable> -DTEST_DIR=<tests/>
#       -DOUT=<scratch dir> -P libreoffice.cmake

execute_process(COMMAND "${TESTS}" WORKING_DIRECTORY "${TEST_DIR}" OUTPUT_QUIET RESULT_VARIABLE tests_failed)
if(tests_failed)
    message(WARNING "Some tests failed; checking the files they wrote anyway")
endif()

# A profile of its own, so a running LibreOffice doesn't get the job.
file(TO_CMAKE_PATH "${OUT}/profile" profile)
if(NOT profile MATCHES "^/")
    set(profile "/${profile}")
endif()

set(failed)
set(n 0)
# The import filter is named, so a file that isn't one is not read as text.
set(filter_odt "writer8")
set(filter_docx "MS Word 2007 XML")
foreach(format odt docx)
    file(GLOB files "${TEST_DIR}/${format}/out/*.${format}")
    if(NOT files)
        message(FATAL_ERROR "No files to check in ${TEST_DIR}/${format}/out")
    endif()
    file(REMOVE_RECURSE "${OUT}/${format}")
    execute_process(
        COMMAND "${SOFFICE}" "-env:UserInstallation=file://${profile}" --headless
            "--infilter=${filter_${format}}" --convert-to txt:Text --outdir "${OUT}/${format}" ${files}
        OUTPUT_QUIET ERROR_QUIET)
    foreach(f IN LISTS files)
        get_filename_component(name "${f}" NAME_WE)
        if(NOT EXISTS "${OUT}/${format}/${name}.txt")
            list(APPEND failed "${f}")
        endif()
        math(EXPR n "${n} + 1")
    endforeach()
endforeach()

if(failed)
    list(JOIN failed "\n  " failed)
    message(FATAL_ERROR "LibreOffice could not open:\n  ${failed}")
endif()
message(STATUS "LibreOffice opened all ${n} files")
