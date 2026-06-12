cmake_minimum_required(VERSION 3.28)

# Catch2 is already available from parent if building in-tree
if(NOT TARGET Catch2::Catch2WithMain)
    find_package(Catch2 3 REQUIRED)
endif()

add_executable(fastrules-json-tests
    test_json_repository.cpp
    test_json_loader.cpp
    test_json_xml_roundtrip.cpp
)
set_property(TARGET fastrules-json-tests PROPERTY FOLDER "Extensions/JSON/Tests")

target_link_libraries(fastrules-json-tests PRIVATE
    fastrules-json
    fastrules-xml
    Catch2::Catch2WithMain
)

include(Catch)
# Skip automatic test discovery on Windows (causes MSB3073 when DLLs not in PATH)
if(NOT WIN32)
    catch_discover_tests(fastrules-json-tests)
endif()
