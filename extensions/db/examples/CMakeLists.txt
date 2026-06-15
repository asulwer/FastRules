cmake_minimum_required(VERSION 3.28)

add_executable(fastrules_db_example db_example.cpp)
target_link_libraries(fastrules_db_example PRIVATE fastrules-db)

# Copy SOCI SQLite3 backend DLL to output directory (needed at runtime on Windows)
if(WIN32)
    # Determine vcpkg installed directory
    if(DEFINED VCPKG_INSTALLED_DIR)
        set(VCPKG_ROOT "${VCPKG_INSTALLED_DIR}")
    else()
        set(VCPKG_ROOT "${CMAKE_BINARY_DIR}/vcpkg_installed")
    endif()
    
    # Debug DLL
    set(SOCI_SQLITE3_DLL_DEBUG "${VCPKG_ROOT}/${VCPKG_TARGET_TRIPLET}/debug/bin/soci_sqlite3_4_0.dll")
    # Release DLL
    set(SOCI_SQLITE3_DLL_RELEASE "${VCPKG_ROOT}/${VCPKG_TARGET_TRIPLET}/bin/soci_sqlite3_4_0.dll")
    
    # Use conditional copy based on actual configuration
    # For Debug builds
    add_custom_command(TARGET fastrules_db_example POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${SOCI_SQLITE3_DLL_DEBUG}"
            $<TARGET_FILE_DIR:fastrules_db_example>/
        COMMENT "Copying SOCI SQLite3 Debug DLL"
    )
    # For Release builds
    add_custom_command(TARGET fastrules_db_example POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${SOCI_SQLITE3_DLL_RELEASE}"
            $<TARGET_FILE_DIR:fastrules_db_example>/
        COMMENT "Copying SOCI SQLite3 Release DLL"
    )
endif()
