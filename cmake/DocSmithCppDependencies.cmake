# DocSmith++ doesn't find its own dependencies when another project uses it.
# The project using it finds them, so it chooses, and sees, every library that
# ends up in what it ships: their licences, versions and vulnerabilities are
# its concern too, even though DocSmith++ links them privately. A new
# dependency in a later release then fails the build with this message rather
# than arriving unnoticed.
#
# The targets need only exist under these names: a project whose package
# manager names them differently can alias its own (add_library(pugixml::pugixml
# ALIAS Pugi::xml)) before using DocSmith++.

# Sets out_message to why DocSmith++ can't be used yet, or to "" if every target
# it links is defined. markdown: whether it is built with DOCSMITHCPP_MARKDOWN.
function(docsmithcpp_check_dependencies markdown out_message)
    set(required pugixml::pugixml miniz::miniz)
    if(markdown)
        list(APPEND required md4c::md4c)
    endif()
    set(missing)
    foreach(target IN LISTS required)
        if(NOT TARGET ${target})
            list(APPEND missing ${target})
        endif()
    endforeach()
    if(NOT missing)
        set(${out_message} "" PARENT_SCOPE)
        return()
    endif()
    list(JOIN required ", " required_list)
    list(JOIN missing ", " missing_list)
    string(CONCAT message
        "DocSmith++ links ${required_list}, and the project using it must define "
        "them first. Missing: ${missing_list}. Find them (e.g. "
        "find_package(pugixml CONFIG REQUIRED)), or alias your own targets to "
        "these names (e.g. add_library(pugixml::pugixml ALIAS Pugi::xml)). "
        "DocSmith++ doesn't find them itself, so that every library in your "
        "build is one you chose.")
    set(${out_message} "${message}" PARENT_SCOPE)
endfunction()
