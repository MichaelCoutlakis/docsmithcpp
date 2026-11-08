# Keeps the code block in README.md the same as examples/showcase.cpp.
#
#   cmake -DSOURCE_DIR=<repo> [-DCHECK=ON] -P cmake/SyncReadmeShowcase.cmake
#
# The block is what lies between the README's showcase markers: the code
# between the example's snippet markers. CHECK fails if the block differs, and
# leaves README.md as it is.

set(begin_marker "<!-- showcase:begin -->")
set(end_marker "<!-- showcase:end -->")

file(READ "${SOURCE_DIR}/examples/showcase.cpp" code)
file(READ "${SOURCE_DIR}/README.md" readme)

# Line endings depend on the checkout, so compare and build with \n.
string(FIND "${readme}" "\r\n" crlf)
string(REPLACE "\r\n" "\n" code "${code}")
string(REPLACE "\r\n" "\n" readme "${readme}")

# The snippet is the lines between the example's markers.
set(snippet_start "// <snippet-start:showcase>")
set(snippet_end "// <snippet-end:showcase>")
string(FIND "${code}" "${snippet_start}" start)
string(FIND "${code}" "${snippet_end}" stop)
if(start EQUAL -1 OR stop EQUAL -1 OR stop LESS start)
    message(FATAL_ERROR "examples/showcase.cpp needs ${snippet_start} and ${snippet_end}")
endif()
string(LENGTH "${snippet_start}" start_length)
math(EXPR first "${start} + ${start_length}")
math(EXPR length "${stop} - ${first}")
string(SUBSTRING "${code}" ${first} ${length} code)
string(STRIP "${code}" code)

string(FIND "${readme}" "${begin_marker}" begin)
string(FIND "${readme}" "${end_marker}" end)
if(begin EQUAL -1 OR end EQUAL -1 OR end LESS begin)
    message(FATAL_ERROR "README.md needs ${begin_marker} and ${end_marker}")
endif()

string(LENGTH "${begin_marker}" begin_length)
math(EXPR after_begin "${begin} + ${begin_length}")
string(SUBSTRING "${readme}" 0 ${after_begin} before)
string(SUBSTRING "${readme}" ${end} -1 after)
set(updated "${before}\n\n```c++\n${code}\n```\n\n${after}")

if(updated STREQUAL readme)
    return()
endif()
if(CHECK)
    message(FATAL_ERROR
        "README.md's showcase differs from examples/showcase.cpp. Build the "
        "example_showcase target with DOCSMITHCPP_SYNC_README=ON (as the dev "
        "preset has it) to update it.")
endif()

if(NOT crlf EQUAL -1)
    string(REPLACE "\n" "\r\n" updated "${updated}")
endif()
file(WRITE "${SOURCE_DIR}/README.md" "${updated}")
message(STATUS "Updated the showcase in README.md")
