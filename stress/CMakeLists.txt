cmake_minimum_required(VERSION 3.28)

# Core stress tests.
# Mirrors the structure of tests/ and examples/ but is gated by
# FASTRULES_BUILD_STRESS_TESTS.

add_executable(fastrules-stress-core core_stress.cpp)
set_property(TARGET fastrules-stress-core PROPERTY FOLDER "Stress/Core")
target_link_libraries(fastrules-stress-core PRIVATE fastrules)

# Include the stress harness headers from the root stress/ directory.
target_include_directories(fastrules-stress-core PRIVATE
    ${CMAKE_SOURCE_DIR}/stress
)

# Copy vcpkg DLLs matching the current build configuration.
if(WIN32 AND VCPKG_INSTALLED_DIR)
    add_custom_command(TARGET fastrules-stress-core POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/$<$<CONFIG:Debug>:debug/>bin"
            "$<TARGET_FILE_DIR:fastrules-stress-core>"
    )
endif()

# Stress tests are heavy, so register them with a long timeout.
add_test(NAME fastrules-stress-core COMMAND fastrules-stress-core --iterations 5 --quiet)
set_tests_properties(fastrules-stress-core PROPERTIES
    LABELS "stress"
    TIMEOUT 120
)
