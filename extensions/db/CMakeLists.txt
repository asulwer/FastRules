cmake_minimum_required(VERSION 3.28)
project(fastrules-db LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(FASTRULES_DB_BACKEND "sqlite3" CACHE STRING "Database backend: postgresql, mysql, sqlite3, odbc")
set_property(CACHE FASTRULES_DB_BACKEND PROPERTY STRINGS postgresql mysql sqlite3 odbc)

set(FASTRULES_CORE_TARGET fastrules)
set(FASTRULES_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/include)

# SOCI must be available
find_package(SOCI COMPONENTS core ${FASTRULES_DB_BACKEND})
if(NOT SOCI_FOUND)
    message(WARNING "SOCI not found. DB extension will not be built. Install SOCI via vcpkg: vcpkg install soci[core,sqlite3]:x64-windows")
    return()
endif()

add_library(fastrules-db STATIC
    src/db_repository.cpp
    include/fastrules/db_bool.hpp
    include/fastrules/db_repository.hpp
)
set_property(TARGET fastrules-db PROPERTY FOLDER "Extensions/DB")

# Organize headers in VS
source_group("Header Files" FILES
    include/fastrules/db_bool.hpp
    include/fastrules/db_repository.hpp
)

target_include_directories(fastrules-db PUBLIC
    include
    ${FASTRULES_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/extensions/include
    ${SOCI_INCLUDE_DIRS}
)

target_link_libraries(fastrules-db PUBLIC
    ${FASTRULES_CORE_TARGET}
    SOCI::soci_core
)

# Link sqlite3 backend directly to ensure it's available
if(TARGET SOCI::soci_sqlite3)
    target_link_libraries(fastrules-db PUBLIC SOCI::soci_sqlite3)
else()
    # Try to find backend library
    find_library(SOCI_SQLITE3_LIB
        NAMES soci_sqlite3_4_0 soci_sqlite3
        PATHS "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/$<$<CONFIG:Debug>:debug/>lib"
              "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/lib"
              "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/debug/lib"
    )
    if(SOCI_SQLITE3_LIB)
        target_link_libraries(fastrules-db PUBLIC ${SOCI_SQLITE3_LIB})
    endif()
endif()

# Examples
option(FASTRULES_DB_BUILD_EXAMPLES "Build DB extension examples" ON)
if(FASTRULES_DB_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Tests
option(FASTRULES_DB_BUILD_TESTS "Build DB extension tests" ON)
if(FASTRULES_DB_BUILD_TESTS AND NOT FASTRULES_IN_TREE_TESTS)
    enable_testing()
    add_subdirectory(tests)
elseif(FASTRULES_DB_BUILD_TESTS AND FASTRULES_IN_TREE_TESTS)
    add_subdirectory(tests)
endif()

# Install
include(GNUInstallDirs)
install(TARGETS fastrules-db
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(DIRECTORY include/fastrules
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
