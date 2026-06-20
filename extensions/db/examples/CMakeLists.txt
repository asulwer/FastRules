cmake_minimum_required(VERSION 3.28)

add_executable(fastrules_db_example db_example.cpp)
set_property(TARGET fastrules_db_example PROPERTY FOLDER "Extensions/DB/Examples")
target_link_libraries(fastrules_db_example PRIVATE fastrules-db)

# Copy SOCI and dependency DLLs to output directory (needed at runtime on Windows)
if(WIN32)
    # Determine vcpkg installed directory
    if(DEFINED VCPKG_INSTALLED_DIR)
        set(VCPKG_ROOT "${VCPKG_INSTALLED_DIR}")
    else()
        set(VCPKG_ROOT "${CMAKE_BINARY_DIR}/vcpkg_installed")
    endif()

    # Get configuration-specific paths - ONLY copy DLLs matching current build config
    set(VCPKG_BIN_DIR "${VCPKG_ROOT}/${VCPKG_TARGET_TRIPLET}/bin")
    set(VCPKG_BIN_DIR_DEBUG "${VCPKG_ROOT}/${VCPKG_TARGET_TRIPLET}/debug/bin")
    set(OUTPUT_DIR "$<TARGET_FILE_DIR:fastrules_db_example>")

    # Only copy DLLs matching the current build configuration to avoid runtime conflicts
    # Debug builds get debug DLLs, Release builds get release DLLs
    add_custom_command(TARGET fastrules_db_example POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<IF:$<CONFIG:Debug>,${VCPKG_BIN_DIR_DEBUG}/soci_core_4_0.dll,${VCPKG_BIN_DIR}/soci_core_4_0.dll>"
            "$<IF:$<CONFIG:Debug>,${VCPKG_BIN_DIR_DEBUG}/soci_sqlite3_4_0.dll,${VCPKG_BIN_DIR}/soci_sqlite3_4_0.dll>"
            "$<IF:$<CONFIG:Debug>,${VCPKG_BIN_DIR_DEBUG}/sqlite3.dll,${VCPKG_BIN_DIR}/sqlite3.dll>"
            "$<IF:$<CONFIG:Debug>,${VCPKG_BIN_DIR_DEBUG}/lua.dll,${VCPKG_BIN_DIR}/lua.dll>"
            "$<IF:$<CONFIG:Debug>,${VCPKG_BIN_DIR_DEBUG}/spdlogd.dll,${VCPKG_BIN_DIR}/spdlog.dll>"
            "${OUTPUT_DIR}/"
        COMMENT "Copying dependency DLLs ($<CONFIG>)"
    )

    # Copy fastrules.dll only when building the core library as a shared library
    if(FASTRULES_BUILD_SHARED)
        set(FASTRULES_DLL "$<TARGET_FILE:fastrules>")
        add_custom_command(TARGET fastrules_db_example POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${FASTRULES_DLL}"
                "${OUTPUT_DIR}/"
            COMMENT "Copying fastrules.dll"
        )
    endif()
endif()
