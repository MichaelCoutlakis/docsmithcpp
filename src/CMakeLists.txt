add_library(docsmithcpp
    "../include/docsmithcpp/docsmith.h"
    "../include/docsmithcpp/box.h"
    "../include/docsmithcpp/nodes_fwd.h"
    "../include/docsmithcpp/inline_nodes.h"
    "../include/docsmithcpp/nodes.h"
    "../include/docsmithcpp/visit.h"
    "../include/docsmithcpp/style.h"
    "../include/docsmithcpp/list.h"
    "../include/docsmithcpp/table.h"
    "../include/docsmithcpp/iostream_writer.h"
    "../include/docsmithcpp/text_doc.h"
    "../include/docsmithcpp/odt/file.h"
    "../include/docsmithcpp/odt/writer.h"

    "text_doc.cpp"
    "odt/file.cpp"
    "odt/writer.cpp"
    "table.cpp")

# Same name in-tree as the installed target, so consumers can switch between
# add_subdirectory and find_package without changing target names.
add_library(docsmith::docsmithcpp ALIAS docsmithcpp)

target_include_directories(docsmithcpp PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
    $<INSTALL_INTERFACE:include>)

target_compile_features(docsmithcpp PUBLIC cxx_std_20)
target_link_libraries(docsmithcpp PUBLIC pugixml::pugixml libzippp::libzippp PRIVATE fmt::fmt)

if(DOCSMITHCPP_DEBUG_FILES)
    target_compile_definitions(docsmithcpp PRIVATE DOCSMITHCPP_DEBUG_FILES)
endif()

install(TARGETS docsmithcpp EXPORT DocSmithCppTargets)
install(DIRECTORY ../include/ DESTINATION include)
