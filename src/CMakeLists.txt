file(GLOB_RECURSE DOCSMITHCPP_SRC CONFIGURE_DEPENDS *.cpp)
file(GLOB_RECURSE DOCSMITHCPP_HEADERS CONFIGURE_DEPENDS ../include/*.hpp)

add_library(docsmithcpp ${DOCSMITHCPP_SRC} ${DOCSMITHCPP_HEADERS} "odt/file.cpp" "odt/xml_writer.cpp")
target_include_directories(docsmithcpp PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
    $<INSTALL_INTERFACE:include>
)

target_compile_features(docsmithcpp PUBLIC cxx_std_17)
target_link_libraries(docsmithcpp PUBLIC pugixml::pugixml libzippp::libzippp)


# Install headers and target
install(TARGETS docsmithcpp EXPORT DocSmithCppTargets)
install(DIRECTORY ../include/ DESTINATION include)