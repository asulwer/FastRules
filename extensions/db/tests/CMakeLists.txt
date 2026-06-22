cmake_minimum_required(VERSION 3.28)

# SQLite backend for testing
if(NOT TARGET SOCI::soci_sqlite3)
    find_package(SOCI REQUIRED COMPONENTS sqlite3)
endif()

add_executable(fastrules-db-tests
    test_main.cpp
    test_db_repository.cpp
    test_db_repository_extended.cpp
)
set_property(TARGET fastrules-db-tests PROPERTY FOLDER "Extensions/DB/Tests")

target_link_libraries(fastrules-db-tests PRIVATE
    fastrules-db
    doctest::doctest
    SOCI::soci_sqlite3
)

# Tests are registered directly with add_test.
# On Windows, prepend the root build output dir so the extension test finds
# fastrules.dll without copying it (avoids parallel-build races), and set
# SOCI_BACKENDS_PATH so the SOCI dynamic loader finds the backend aliases.
if(WIN32)
    add_test(NAME fastrules-db-tests
        COMMAND ${CMAKE_COMMAND} -E env "PATH=$<TARGET_FILE_DIR:fastrules-db-tests>" "SOCI_BACKENDS_PATH=$<TARGET_FILE_DIR:fastrules-db-tests>" $<TARGET_FILE:fastrules-db-tests>
    )
else()
    add_test(NAME fastrules-db-tests COMMAND fastrules-db-tests)
endif()

# On Windows, copy runtime dependencies next to the test executable so they are found
# without relying on PATH. Also create SOCI backend aliases (soci_core_4_0.dll -> soci_core.dll)
# because SOCI's dynamic backend loader looks for the unversioned name.
# Only vcpkg DLLs are copied here; fastrules.dll is in the root output dir and
# added to PATH in the add_test command above.
if(WIN32)
    set(DB_TESTS_DIR "$<TARGET_FILE_DIR:fastrules-db-tests>")
    # Copy only DLLs matching the current build configuration to avoid mixing
    # Debug and Release runtimes (which can break SQLite/SOCI at runtime).
    set(_DB_TEST_DLL_SRC_DIRS
        "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/$<$<CONFIG:Debug>:debug/>bin"
    )
    string(JOIN "|" _DB_TEST_DLL_SRC_DIRS_STR ${_DB_TEST_DLL_SRC_DIRS})

    add_custom_command(TARGET fastrules-db-tests POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            -D SRC_DIRS="${_DB_TEST_DLL_SRC_DIRS_STR}"
            -D DST_DIR="${DB_TESTS_DIR}"
            -D ALIASES="soci_core_4_0.dll:soci_core.dll|soci_core_4_0.dll:libsoci_core.dll|soci_sqlite3_4_0.dll:soci_sqlite3.dll|soci_sqlite3_4_0.dll:libsoci_sqlite3.dll"
            -P "${CMAKE_SOURCE_DIR}/cmake/copy_test_dlls.cmake"
        COMMENT "Copying runtime DLLs to fastrules-db-tests output directory"
    )
endif()
