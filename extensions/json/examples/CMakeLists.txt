cmake_minimum_required(VERSION 3.28)

add_executable(fastrules_json_example json_example.cpp)
set_property(TARGET fastrules_json_example PROPERTY FOLDER "Extensions/JSON/Examples")
target_link_libraries(fastrules_json_example PRIVATE fastrules-json)
