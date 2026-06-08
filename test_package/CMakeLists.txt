cmake_minimum_required(VERSION 3.28)
project(fastrules_test_package LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(fastrules REQUIRED CONFIG)

add_executable(test_package src/test_package.cpp)
target_link_libraries(test_package PRIVATE fastrules::fastrules)
