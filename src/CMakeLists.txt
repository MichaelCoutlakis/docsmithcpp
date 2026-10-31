
if(DOCSMITHCPP_BUILD_MINIMAL_USAGE)

    add_executable(docsmithcpp
        "odt/basic_main.cpp"
    )

else()

add_library(docsmithcpp
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
endif()

target_include_directories(docsmithcpp PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
    $<INSTALL_INTERFACE:include>
)

target_compile_features(docsmithcpp PUBLIC cxx_std_20)
target_link_libraries(docsmithcpp PUBLIC pugixml::pugixml libzippp::libzippp PRIVATE fmt::fmt)


# Install headers and target
install(TARGETS docsmithcpp EXPORT DocSmithCppTargets)
install(DIRECTORY ../include/ DESTINATION include)
