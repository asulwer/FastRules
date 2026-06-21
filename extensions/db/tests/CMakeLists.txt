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

# On Windows, copy runtime dependencies next to the test executable so they are
# found without relying on PATH. Also create SOCI backend aliases
# (soci_core_4_0.dll -> soci_core.dll) because SOCI's dynamic backend loader
# looks for the unversioned name. fastrules.dll is copied from the root output
# dir so the DB test can run directly.
# Note: run the DB test with SOCI_BACKENDS_PATH set to the test directory
# (e.g. set SOCI_BACKENDS_PATH=build_vs\extensions\db\tests\Debug).
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

    add_custom_command(TARGET fastrules-db-tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:fastrules>"
            "$<TARGET_FILE_DIR:fastrules-db-tests>"
        COMMENT "Copying fastrules.dll next to fastrules-db-tests"
    )
endif()
