cmake_minimum_required(VERSION 3.28)

if(NOT TARGET Catch2::Catch2WithMain)
    find_package(Catch2 3 REQUIRED)
endif()

add_executable(fastrules-xml-tests
    test_xml_repository.cpp
)
set_property(TARGET fastrules-xml-tests PROPERTY FOLDER "Extensions/XML/Tests")

target_link_libraries(fastrules-xml-tests PRIVATE
    fastrules-xml
    Catch2::Catch2WithMain
)

include(Catch)
catch_discover_tests(fastrules-xml-tests)
