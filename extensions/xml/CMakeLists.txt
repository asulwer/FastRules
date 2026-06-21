cmake_minimum_required(VERSION 3.28)
project(fastrules-xml LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# pugixml is provided by vcpkg
find_package(pugixml CONFIG REQUIRED)

set(FASTRULES_CORE_TARGET fastrules)
set(FASTRULES_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/include)

add_library(fastrules-xml STATIC
    src/xml_repository.cpp
    src/xml_loader.cpp
    src/xml_serialization.cpp
    include/fastrules/xml_loader.hpp
    include/fastrules/xml_repository.hpp
    include/fastrules/xml_serialization.hpp
)
set_property(TARGET fastrules-xml PROPERTY FOLDER "Extensions/XML")

# Organize headers in VS
source_group("Header Files" FILES
    include/fastrules/xml_loader.hpp
    include/fastrules/xml_repository.hpp
    include/fastrules/xml_serialization.hpp
)

target_include_directories(fastrules-xml PUBLIC
    include
    ${FASTRULES_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/extensions/include
)

target_link_libraries(fastrules-xml PUBLIC
    ${FASTRULES_CORE_TARGET}
    pugixml::pugixml
)

# Examples
option(FASTRULES_XML_BUILD_EXAMPLES "Build XML extension examples" ON)
if(FASTRULES_XML_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Tests
option(FASTRULES_XML_BUILD_TESTS "Build XML extension tests" ON)
if(FASTRULES_XML_BUILD_TESTS AND NOT FASTRULES_IN_TREE_TESTS)
    enable_testing()
    add_subdirectory(tests)
elseif(FASTRULES_XML_BUILD_TESTS AND FASTRULES_IN_TREE_TESTS)
    add_subdirectory(tests)
endif()

# Install
include(GNUInstallDirs)
install(TARGETS fastrules-xml
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(DIRECTORY include/fastrules
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
