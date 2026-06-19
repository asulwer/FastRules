cmake_minimum_required(VERSION 3.28)

# doctest is already available from parent if building in-tree
if(NOT TARGET doctest::doctest)
    find_package(doctest REQUIRED)
endif()

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

# Tests are registered directly with add_test
add_test(NAME fastrules-db-tests COMMAND fastrules-db-tests)
