add_library(docsmithcpp 
    "../include/docsmithcpp/element.h"
    "../include/docsmithcpp/iostream_writer.h"
    "../include/docsmithcpp/text_doc.h"
    "../include/docsmithcpp/odt/file.h"
    "../include/docsmithcpp/odt/writer.h"

    "odt/file.cpp" 
    "odt/writer.cpp")

target_include_directories(docsmithcpp PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
    $<INSTALL_INTERFACE:include>
)

target_compile_features(docsmithcpp PUBLIC cxx_std_17)
target_link_libraries(docsmithcpp PUBLIC pugixml::pugixml libzippp::libzippp)


# Install headers and target
install(TARGETS docsmithcpp EXPORT DocSmithCppTargets)
install(DIRECTORY ../include/ DESTINATION include)