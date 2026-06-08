cmake_minimum_required(VERSION 3.28)

add_executable(fastrules_xml_example xml_example.cpp)
set_property(TARGET fastrules_xml_example PROPERTY FOLDER "Extensions/XML/Examples")
target_link_libraries(fastrules_xml_example PRIVATE fastrules-xml)
