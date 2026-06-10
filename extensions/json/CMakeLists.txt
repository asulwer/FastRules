cmake_minimum_required(VERSION 3.28)
project(fastrules-json LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Determine if we're building standalone or as part of fastrules
if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    # Standalone build — need to find fastrules
    find_package(fastrules REQUIRED CONFIG)
    find_package(nlohmann_json REQUIRED)
    set(FASTRULES_CORE_TARGET fastrules::fastrules)
    set(FASTRULES_INCLUDE_DIR "")
else()
    # Building as part of fastrules — use the in-tree target
    set(FASTRULES_CORE_TARGET fastrules)
    set(FASTRULES_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/include)
    # nlohmann/json is already available from parent
    if(NOT TARGET nlohmann_json::nlohmann_json)
        # Try to find via CMake config first
        find_package(nlohmann_json QUIET CONFIG)
        if(NOT nlohmann_json_FOUND)
            # Fallback: create alias from the header-only target fetched by parent
            if(TARGET nlohmann_json)
                add_library(nlohmann_json::nlohmann_json ALIAS nlohmann_json)
            else()
                find_package(nlohmann_json REQUIRED)
            endif()
        endif()
    endif()
endif()

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

# Add nlohmann/json include directory explicitly for IntelliSense
target_include_directories(fastrules-json SYSTEM PUBLIC
    ${nlohmann_json_SOURCE_DIR}/include
    ${nlohmann_json_SOURCE_DIR}/single_include
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
option(FASTRULES_JSON_BUILD_TESTS "Build JSON extension tests" ON)
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
