cmake_minimum_required(VERSION 3.28)

add_executable(fastrules-stress-xml xml_stress.cpp)
set_property(TARGET fastrules-stress-xml PROPERTY FOLDER "Extensions/XML/Stress")
target_link_libraries(fastrules-stress-xml PRIVATE fastrules-xml)

target_include_directories(fastrules-stress-xml PRIVATE
    ${CMAKE_SOURCE_DIR}/stress
    ${CMAKE_SOURCE_DIR}/extensions/include
)

if(WIN32 AND VCPKG_INSTALLED_DIR)
    add_custom_command(TARGET fastrules-stress-xml POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/$<$<CONFIG:Debug>:debug/>bin"
            "$<TARGET_FILE_DIR:fastrules-stress-xml>"
    )
endif()

add_test(NAME fastrules-stress-xml COMMAND fastrules-stress-xml --iterations 5 --quiet)
set_tests_properties(fastrules-stress-xml PROPERTIES
    LABELS "stress"
    TIMEOUT 120
)
