cmake_minimum_required(VERSION 3.28)

# doctest is already available from parent if building in-tree
if(NOT TARGET doctest::doctest)
    find_package(doctest REQUIRED)
endif()

add_executable(fastrules-json-tests
    test_main.cpp
    test_json_repository.cpp
    test_json_loader.cpp
    test_json_xml_roundtrip.cpp
    test_json_repository_extended.cpp
)
set_property(TARGET fastrules-json-tests PROPERTY FOLDER "Extensions/JSON/Tests")

target_link_libraries(fastrules-json-tests PRIVATE
    fastrules-json
    fastrules-xml
    doctest::doctest
)

# Tests are registered directly with add_test
add_test(NAME fastrules-json-tests COMMAND fastrules-json-tests)
