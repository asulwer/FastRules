cmake_minimum_required(VERSION 3.28)
project(fastrules-xml LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Determine if we're building standalone or as part of fastrules
if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    # Standalone build — need to find fastrules
    find_package(fastrules REQUIRED CONFIG)
    # Fetch pugixml
    include(FetchContent)
    FetchContent_Declare(
        pugixml
        GIT_REPOSITORY https://github.com/zeux/pugixml.git
        GIT_TAG v1.14
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(pugixml)
    set(FASTRULES_CORE_TARGET fastrules::fastrules)
    set(FASTRULES_INCLUDE_DIR "")
else()
    # Building as part of fastrules — use the in-tree target
    set(FASTRULES_CORE_TARGET fastrules)
    set(FASTRULES_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/include)
    # Fetch pugixml if not available
    if(NOT TARGET pugixml::pugixml)
        include(FetchContent)
        FetchContent_Declare(
            pugixml
            GIT_REPOSITORY https://github.com/zeux/pugixml.git
            GIT_TAG v1.14
            GIT_SHALLOW TRUE
        )
        FetchContent_MakeAvailable(pugixml)
        
        # Organize pugixml in VS solution
        if(TARGET pugixml-static)
            set_target_properties(pugixml-static PROPERTIES FOLDER "ThirdParty")
        endif()
        if(TARGET pugixml)
            set_target_properties(pugixml PROPERTIES FOLDER "ThirdParty")
        endif()
    endif()
endif()

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

# Add pugixml include directory explicitly for IntelliSense
target_include_directories(fastrules-xml SYSTEM PUBLIC
    ${pugixml_SOURCE_DIR}/src
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
if(FASTRULES_XML_BUILD_TESTS)
    enable_testing()
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
