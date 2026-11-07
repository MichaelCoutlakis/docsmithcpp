add_library(docsmithcpp
    "../include/docsmithcpp/docsmith.h"
    "../include/docsmithcpp/box.h"
    "../include/docsmithcpp/nodes_fwd.h"
    "../include/docsmithcpp/inline_nodes.h"
    "../include/docsmithcpp/nodes.h"
    "../include/docsmithcpp/page.h"
    "../include/docsmithcpp/visit.h"
    "../include/docsmithcpp/style.h"
    "../include/docsmithcpp/list.h"
    "../include/docsmithcpp/table.h"
    "../include/docsmithcpp/iostream_writer.h"
    "../include/docsmithcpp/resource.h"
    "../include/docsmithcpp/metadata.h"
    "../include/docsmithcpp/text_doc.h"
    "../include/docsmithcpp/odt/file.h"
    "../include/docsmithcpp/html/file.h"
    "../include/docsmithcpp/md/file.h"
    "../include/docsmithcpp/docx/file.h"

    # Private: the archive and XML details stay out of the public headers.
    "bytes.h"
    "bytes.cpp"
    "format.h"
    "list_numbers.h"
    "outline.h"
    "zip/archive.h"
    "zip/archive.cpp"
    "odf/package.h"
    "odf/package.cpp"
    "odf/meta.h"
    "odf/meta.cpp"
    "odt/styles.h"
    "odt/styles.cpp"
    "odt/writer.h"
    "odt/writer.cpp"
    "odt/file.cpp"
    "html/file.cpp"
    "resource_plan.h"
    "resource_plan.cpp"
    "md/names.h"
    "md/file.cpp"
    "image_size.h"
    "image_size.cpp"
    "xml/namespaces.h"
    "xml/namespaces.cpp"
    "opc/package.h"
    "opc/package.cpp"
    "docx/ooxml.h"
    "docx/state.h"
    "docx/styles.h"
    "docx/styles.cpp"
    "docx/meta.h"
    "docx/meta.cpp"
    "docx/writer.h"
    "docx/writer.cpp"
    "docx/reader.h"
    "docx/reader.cpp"
    "docx/file.cpp"

    "text_doc.cpp"
    "table.cpp")

# Same name in-tree as the installed target, so consumers can switch between
# add_subdirectory and find_package without changing target names.
add_library(docsmith::docsmithcpp ALIAS docsmithcpp)

target_include_directories(docsmithcpp
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_features(docsmithcpp PUBLIC cxx_std_20)

# All private: no public header includes them. The project using DocSmith++
# still defines these targets itself (cmake/DocSmithCppDependencies.cmake).
target_link_libraries(docsmithcpp PRIVATE pugixml::pugixml miniz::miniz)

# Markdown output needs nothing; input uses md4c. The definition is public, so
# md/file.h declares parse() and load() for consumers too.
if(DOCSMITHCPP_MARKDOWN)
    target_sources(docsmithcpp PRIVATE "md/parser.cpp")
    target_link_libraries(docsmithcpp PRIVATE md4c::md4c)
    target_compile_definitions(docsmithcpp PUBLIC DOCSMITHCPP_MARKDOWN)
endif()

if(DOCSMITHCPP_DEBUG_FILES)
    target_compile_definitions(docsmithcpp PRIVATE DOCSMITHCPP_DEBUG_FILES)
endif()

install(TARGETS docsmithcpp EXPORT DocSmithCppTargets)
install(DIRECTORY ../include/ DESTINATION include)
