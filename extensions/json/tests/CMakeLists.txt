cmake_minimum_required(VERSION 3.28)

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

# On Windows, copy runtime DLLs next to the test executable so it can be run
# directly without relying on PATH. Only vcpkg DLLs are copied; fastrules.dll
# lives in the root output dir and is copied below.
if(WIN32)
    # Only copy DLLs matching the current build configuration.
    set(_JSON_TEST_DLL_SRC_DIR
        "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/$<$<CONFIG:Debug>:debug/>bin"
    )

    add_custom_command(TARGET fastrules-json-tests POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            -D SRC_DIRS="${_JSON_TEST_DLL_SRC_DIR}"
            -D DST_DIR="$<TARGET_FILE_DIR:fastrules-json-tests>"
            -P "${CMAKE_SOURCE_DIR}/cmake/copy_test_dlls.cmake"
        COMMENT "Copying runtime DLLs to fastrules-json-tests output directory"
    )

    add_custom_command(TARGET fastrules-json-tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:fastrules>"
            "$<TARGET_FILE_DIR:fastrules-json-tests>"
        COMMENT "Copying fastrules.dll next to fastrules-json-tests"
    )
endif()
