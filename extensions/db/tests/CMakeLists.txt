cmake_minimum_required(VERSION 3.28)

if(NOT TARGET Catch2::Catch2WithMain)
    find_package(Catch2 3 REQUIRED)
endif()

# SQLite backend for testing
if(NOT TARGET SOCI::soci_sqlite3)
    find_package(SOCI REQUIRED COMPONENTS sqlite3)
endif()

add_executable(fastrules-db-tests
    test_db_repository.cpp
)

target_link_libraries(fastrules-db-tests PRIVATE
    fastrules-db
    Catch2::Catch2WithMain
    ${SOCI_SQLITE3_LIB}
)

include(Catch)
catch_discover_tests(fastrules-db-tests)
