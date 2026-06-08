cmake_minimum_required(VERSION 3.28)

add_executable(fastrules_db_example db_example.cpp)
target_link_libraries(fastrules_db_example PRIVATE fastrules-db)
