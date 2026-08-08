cmake_minimum_required(VERSION 3.28)
project(fastrules-json LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# nlohmann/json is provided by vcpkg
find_package(nlohmann_json CONFIG REQUIRED)

set(FASTRULES_CORE_TARGET fastrules)
set(FASTRULES_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/include)

add_library(fastrules-json STATIC
    src/json_loader.cpp
    src/json_repository.cpp
    src/json_serialization.cpp
    include/fastrules/json_loader.hpp
    include/fastrules/json_repository.hpp
    include/fastrules/json_serialization.hpp
)
set_property(TARGET fastrules-json PROPERTY FOLDER "Extensions/JSON")

# Organize headers in VS
source_group("Header Files" FILES
    include/fastrules/json_loader.hpp
    include/fastrules/json_repository.hpp
    include/fastrules/json_serialization.hpp
)

target_compile_definitions(fastrules-json PUBLIC FASTRULES_HAS_JSON_LOADER)

target_include_directories(fastrules-json PUBLIC
    include
    ${FASTRULES_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/extensions/include
)

target_link_libraries(fastrules-json PUBLIC
    ${FASTRULES_CORE_TARGET}
    nlohmann_json::nlohmann_json
)

# Examples
option(FASTRULES_JSON_BUILD_EXAMPLES "Build JSON extension examples" ON)
if(FASTRULES_JSON_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Tests
# Default to the core project's setting rather than ON. Extension tests need
# doctest, which a consumer build (FASTRULES_BUILD_TESTS=OFF - e.g. the Conan
# package, which sets with_tests=False) does not provide. Defaulting to ON made
# every such build add these targets and then fail linking doctest::doctest.
if(NOT DEFINED FASTRULES_BUILD_TESTS)
    set(FASTRULES_BUILD_TESTS OFF)
endif()
option(FASTRULES_JSON_BUILD_TESTS "Build JSON extension tests" ${FASTRULES_BUILD_TESTS})
if(FASTRULES_JSON_BUILD_TESTS AND NOT FASTRULES_IN_TREE_TESTS)
    enable_testing()
    add_subdirectory(tests)
elseif(FASTRULES_JSON_BUILD_TESTS AND FASTRULES_IN_TREE_TESTS)
    add_subdirectory(tests)
endif()

# Install
include(GNUInstallDirs)
install(TARGETS fastrules-json
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(DIRECTORY include/fastrules
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
