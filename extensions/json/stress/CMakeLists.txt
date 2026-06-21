cmake_minimum_required(VERSION 3.28)

add_executable(fastrules-stress-json json_stress.cpp)
set_property(TARGET fastrules-stress-json PROPERTY FOLDER "Extensions/JSON/Stress")
target_link_libraries(fastrules-stress-json PRIVATE fastrules-json)

target_include_directories(fastrules-stress-json PRIVATE
    ${CMAKE_SOURCE_DIR}/stress
    ${CMAKE_SOURCE_DIR}/extensions/include
)

if(WIN32 AND VCPKG_INSTALLED_DIR)
    add_custom_command(TARGET fastrules-stress-json POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/$<$<CONFIG:Debug>:debug/>bin"
            "$<TARGET_FILE_DIR:fastrules-stress-json>"
    )
endif()

add_test(NAME fastrules-stress-json COMMAND fastrules-stress-json --iterations 5 --quiet)
set_tests_properties(fastrules-stress-json PROPERTIES
    LABELS "stress"
    TIMEOUT 120
)
