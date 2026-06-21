cmake_minimum_required(VERSION 3.28)

add_executable(fastrules-stress-db db_stress.cpp)
set_property(TARGET fastrules-stress-db PROPERTY FOLDER "Extensions/DB/Stress")
target_link_libraries(fastrules-stress-db PRIVATE fastrules-db)

target_include_directories(fastrules-stress-db PRIVATE
    ${CMAKE_SOURCE_DIR}/stress
    ${CMAKE_SOURCE_DIR}/extensions/include
)

if(WIN32 AND VCPKG_INSTALLED_DIR)
    add_custom_command(TARGET fastrules-stress-db POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/$<$<CONFIG:Debug>:debug/>bin"
            "$<TARGET_FILE_DIR:fastrules-stress-db>"
    )
endif()

add_test(NAME fastrules-stress-db COMMAND fastrules-stress-db --iterations 5 --quiet)
set_tests_properties(fastrules-stress-db PROPERTIES
    LABELS "stress"
    TIMEOUT 120
)
