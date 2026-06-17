cmake_minimum_required(VERSION 3.28)

# doctest is already available from parent if building in-tree
if(NOT TARGET doctest::doctest)
    find_package(doctest REQUIRED)
endif()

add_executable(fastrules-xml-tests
    test_main.cpp
    test_xml_repository.cpp
)
set_property(TARGET fastrules-xml-tests PROPERTY FOLDER "Extensions/XML/Tests")

target_link_libraries(fastrules-xml-tests PRIVATE
    fastrules-xml
    doctest::doctest
)

# Tests are registered directly with add_test
add_test(NAME fastrules-xml-tests COMMAND fastrules-xml-tests)
