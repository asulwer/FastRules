cmake_minimum_required(VERSION 3.28)

add_executable(fastrules-xml-tests
    test_main.cpp
    test_xml_repository.cpp
    test_xml_repository_extended.cpp
)
set_property(TARGET fastrules-xml-tests PROPERTY FOLDER "Extensions/XML/Tests")

target_link_libraries(fastrules-xml-tests PRIVATE
    fastrules-xml
    doctest::doctest
)

# On Windows, copy runtime DLLs next to the test executable so it can be run
# directly without relying on PATH. Only vcpkg DLLs are copied; fastrules.dll
# lives in the root output dir and is copied below.
if(WIN32)
    # Only copy DLLs matching the current build configuration.
    set(_XML_TEST_DLL_SRC_DIR
        "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/$<$<CONFIG:Debug>:debug/>bin"
    )

    add_custom_command(TARGET fastrules-xml-tests POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            -D SRC_DIRS="${_XML_TEST_DLL_SRC_DIR}"
            -D DST_DIR="$<TARGET_FILE_DIR:fastrules-xml-tests>"
            -P "${CMAKE_SOURCE_DIR}/cmake/copy_test_dlls.cmake"
        COMMENT "Copying runtime DLLs to fastrules-xml-tests output directory"
    )

    add_custom_command(TARGET fastrules-xml-tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:fastrules>"
            "$<TARGET_FILE_DIR:fastrules-xml-tests>"
        COMMENT "Copying fastrules.dll next to fastrules-xml-tests"
    )
endif()
