cmake_minimum_required(VERSION 3.28)
project(fastrules VERSION 0.1.0 LANGUAGES C CXX)

# CMP0091: MSVC runtime library flags are selected by the MSVC_RUNTIME_LIBRARY target property
# Must be set before any targets are created
cmake_policy(SET CMP0091 NEW)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable solution folders for Visual Studio
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Suppress CMake auto-generated folders
set(CMAKE_SUPPRESS_REGENERATION ON)
set(CMAKE_SKIP_INSTALL_ALL_DEPENDENCY ON)

# Set MSVC runtime library globally before any targets are created
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

# Options
# Enable vcpkg manifest mode when vcpkg.json is present
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg.json")
    set(VCPKG_MANIFEST_MODE ON CACHE BOOL "" FORCE)
    set(VCPKG_APPLOCAL_DEPS OFF)
    message(STATUS "vcpkg manifest mode enabled")
endif()

option(FASTRULES_BUILD_TESTS "Build tests" ON)
option(FASTRULES_BUILD_EXAMPLES "Build examples" ON)
option(FASTRULES_BUILD_EXTENSIONS "Build optional persistence extensions" OFF)
option(FASTRULES_BUILD_DB "Build database persistence extension (requires SOCI)" OFF)
option(FASTRULES_BUILD_COVERAGE "Build with coverage instrumentation" OFF)
option(FASTRULES_USE_LUAJIT "Use LuaJIT instead of PUC-Rio Lua (requires manual build on Windows)" OFF)
# LuaBridge3 backend only
set(FASTRULES_LUA_BACKEND "luabridge3" CACHE STRING "Lua binding backend")

# Suppress CTest dashboard targets from all dependencies
set_property(GLOBAL PROPERTY CTEST_TARGETS_ADDED 1)

# Third-party dependencies via CPM or FetchContent
include(FetchContent)

# nlohmann/json
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
)

FetchContent_Declare(
    lua
    GIT_REPOSITORY https://github.com/lua/lua.git
    GIT_TAG v5.4.7
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
)

FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.14.1
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
)

FetchContent_Declare(
    catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.5.2
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
)

FetchContent_Declare(
    luabridge3
    GIT_REPOSITORY https://github.com/kunitoki/LuaBridge3.git
    GIT_TAG master
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
)

# LuaJIT option — fetch and build automatically if requested
if(FASTRULES_USE_LUAJIT)
    FetchContent_Declare(
        luajit
        GIT_REPOSITORY https://github.com/LuaJIT/LuaJIT.git
        GIT_TAG v2.1
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
    )
    FetchContent_MakeAvailable(luajit)
    
    # On Windows with MSVC, LuaJIT must be built with its own msvcbuild.bat
    # because it requires buildvm (a build tool) to generate bytecode headers.
    if(MSVC)
        set(LUAJIT_BUILD_DIR "${luajit_SOURCE_DIR}/src")
        set(LUAJIT_LIB "${LUAJIT_BUILD_DIR}/lua51.lib")
        
        # Find vcvarsall.bat to set up the VS build environment
        find_file(VCVARSALL_BAT NAMES vcvarsall.bat
            PATHS
                "C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build"
                "C:/Program Files/Microsoft Visual Studio/2022/Professional/VC/Auxiliary/Build"
                "C:/Program Files/Microsoft Visual Studio/2022/Enterprise/VC/Auxiliary/Build"
                "C:/Program Files (x86)/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build"
                "C:/Program Files (x86)/Microsoft Visual Studio/2022/Professional/VC/Auxiliary/Build"
                "C:/Program Files (x86)/Microsoft Visual Studio/2022/Enterprise/VC/Auxiliary/Build"
        )
        
        if(NOT VCVARSALL_BAT)
            message(FATAL_ERROR "Cannot find vcvarsall.bat. LuaJIT requires Visual Studio build tools.")
        endif()
        
        # Determine architecture
        if(CMAKE_SIZEOF_VOID_P EQUAL 8)
            set(LUAJIT_ARCH_ARG "x64")
        else()
            set(LUAJIT_ARCH_ARG "x86")
        endif()
        
        # Build LuaJIT static library using msvcbuild.bat
        # The build generates: lua51.lib, lua51.dll, luajit.exe in src/
        # Note: Must call vcvarsall.bat and msvcbuild.bat in the same cmd process
        # because vcvarsall.bat sets environment variables needed by msvcbuild.bat
        set(LUAJIT_BUILD_SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/build_luajit.bat")
        file(WRITE ${LUAJIT_BUILD_SCRIPT} "@echo off\n")
        file(APPEND ${LUAJIT_BUILD_SCRIPT} "call \"${VCVARSALL_BAT}\" ${LUAJIT_ARCH_ARG}\n")
        file(APPEND ${LUAJIT_BUILD_SCRIPT} "cd /d \"${LUAJIT_BUILD_DIR}\"\n")
        file(APPEND ${LUAJIT_BUILD_SCRIPT} "msvcbuild.bat static lua52compat\n")
        
        add_custom_command(
            OUTPUT ${LUAJIT_LIB}
            COMMAND cmd /c "${LUAJIT_BUILD_SCRIPT}"
            WORKING_DIRECTORY ${LUAJIT_BUILD_DIR}
            COMMENT "Building LuaJIT static library with msvcbuild.bat..."
            VERBATIM
        )
        
        # Create a custom target that depends on the built library
        add_custom_target(luajit_build DEPENDS ${LUAJIT_LIB})
        
        # Create an imported static library target
        add_library(lua_static STATIC IMPORTED GLOBAL)
        set_target_properties(lua_static PROPERTIES
            IMPORTED_LOCATION "${LUAJIT_LIB}"
            INTERFACE_INCLUDE_DIRECTORIES "${LUAJIT_BUILD_DIR}"
        )
        add_dependencies(lua_static luajit_build)
        
        target_compile_definitions(lua_static INTERFACE LUAJIT_ENABLE_LUA52COMPAT)
        
        message(STATUS "Using LuaJIT (MSVC build via msvcbuild.bat)")
        message(STATUS "  LuaJIT source: ${luajit_SOURCE_DIR}")
        message(STATUS "  LuaJIT lib: ${LUAJIT_LIB}")
    else()
        # Non-Windows: use make
        set(LUAJIT_BUILD_DIR "${luajit_SOURCE_DIR}")
        set(LUAJIT_LIB "${LUAJIT_BUILD_DIR}/src/libluajit.a")
        
        include(ExternalProject)
        ExternalProject_Add(
            luajit_external
            SOURCE_DIR ${LUAJIT_BUILD_DIR}
            CONFIGURE_COMMAND ""
            BUILD_COMMAND ${CMAKE_MAKE_PROGRAM} -C "${LUAJIT_BUILD_DIR}" amalg BUILDMODE=static XCFLAGS=-DLUAJIT_ENABLE_LUA52COMPAT
            INSTALL_COMMAND ""
            BUILD_BYPRODUCTS ${LUAJIT_LIB}
        )
        
        add_library(lua_static STATIC IMPORTED GLOBAL)
        set_target_properties(lua_static PROPERTIES
            IMPORTED_LOCATION "${LUAJIT_LIB}"
            INTERFACE_INCLUDE_DIRECTORIES "${LUAJIT_BUILD_DIR}/src"
        )
        add_dependencies(lua_static luajit_external)
        target_compile_definitions(lua_static INTERFACE LUAJIT_ENABLE_LUA52COMPAT)
        
        message(STATUS "Using LuaJIT (make build)")
    endif()
    
    # Fetch dependencies
    FetchContent_MakeAvailable(json luabridge3 catch2)

    # Organize external targets in VS solution
    if(TARGET nlohmann_json)
        set_target_properties(nlohmann_json PROPERTIES FOLDER "ThirdParty")
    endif()
    if(TARGET spdlog)
        set_target_properties(spdlog PROPERTIES FOLDER "ThirdParty")
    endif()
    if(TARGET Catch2)
        set_target_properties(Catch2 PROPERTIES FOLDER "ThirdParty")
    endif()
    if(TARGET Catch2WithMain)
        set_target_properties(Catch2WithMain PROPERTIES FOLDER "ThirdParty")
    endif()
    if(TARGET luabridge3)
        set_target_properties(luabridge3 PROPERTIES FOLDER "ThirdParty")
    endif()
else()
    FetchContent_MakeAvailable(lua json luabridge3 spdlog catch2)

    # Organize external targets in VS solution
    if(TARGET nlohmann_json)
        set_target_properties(nlohmann_json PROPERTIES FOLDER "ThirdParty")
    endif()
    if(TARGET spdlog)
        set_target_properties(spdlog PROPERTIES FOLDER "ThirdParty")
    endif()
    if(TARGET Catch2)
        set_target_properties(Catch2 PROPERTIES FOLDER "ThirdParty")
    endif()
    if(TARGET Catch2WithMain)
        set_target_properties(Catch2WithMain PROPERTIES FOLDER "ThirdParty")
    endif()
    if(TARGET luabridge3)
        set_target_properties(luabridge3 PROPERTIES FOLDER "ThirdParty")
    endif()
endif()

# If we chose PUC-Rio Lua (not LuaJIT)
if(NOT FASTRULES_USE_LUAJIT)
    # Check if lua was fetched
    if(NOT DEFINED lua_SOURCE_DIR OR NOT EXISTS ${lua_SOURCE_DIR})
        message(FATAL_ERROR "Lua source not available. Make sure FetchContent succeeded.")
    endif()
    
    # For PUC-Rio Lua, create a library target manually
    file(GLOB LUA_SRCS ${lua_SOURCE_DIR}/*.c)
    # Exclude luac.c and lua.c (the standalone interpreter/compiler)
    list(FILTER LUA_SRCS EXCLUDE REGEX "luac?\\.c$")

    add_library(lua_static STATIC ${LUA_SRCS})
    set_target_properties(lua_static PROPERTIES FOLDER "ThirdParty")
    target_include_directories(lua_static PUBLIC ${lua_SOURCE_DIR})
endif()

# === fastrules library ===
set(FASTRULES_CORE_SOURCES
    src/action_callback.cpp
    src/async_workflow.cpp
    src/lua_engine.cpp
    src/rule.cpp
    src/rule_context.cpp
    src/workflow.cpp
    src/execution_tracer.cpp
    src/parameter_validator.cpp
    src/expression_validator.cpp
    src/rate_limiter.cpp
    src/performance_counters.cpp
    src/aot_compiler.cpp
    src/rule_versioning.cpp
)

# Backend-specific sources
list(APPEND FASTRULES_CORE_SOURCES src/lua_backend_factory.cpp src/lua_backend_luabridge.cpp)

# Collect public headers so they appear in Visual Studio projects
set(FASTRULES_PUBLIC_HEADERS
    include/fastrules/action_callback.hpp
    include/fastrules/aot_compiler.hpp
    include/fastrules/async_workflow.hpp
    include/fastrules/execution_tracer.hpp
    include/fastrules/expression_validator.hpp
    include/fastrules/fastrules.hpp
    include/fastrules/logger.hpp
    include/fastrules/lua_backend.hpp
    include/fastrules/lua_backend_luabridge.hpp
    include/fastrules/lua_engine.hpp
    include/fastrules/parameter_validator.hpp
    include/fastrules/performance_counters.hpp
    include/fastrules/rate_limiter.hpp
    include/fastrules/rule.hpp
    include/fastrules/rule_context.hpp
    include/fastrules/rule_result.hpp
    include/fastrules/rule_versioning.hpp
    include/fastrules/streaming_result.hpp
    include/fastrules/type_registry.hpp
    include/fastrules/workflow.hpp
)

# ============================================================================
# fastrules library
# ============================================================================
add_library(fastrules ${FASTRULES_CORE_SOURCES})
set_property(TARGET fastrules PROPERTY FOLDER "Core")

# Mark headers as PUBLIC_HEADER for install and source_group
set_target_properties(fastrules PROPERTIES PUBLIC_HEADER "${FASTRULES_PUBLIC_HEADERS}")

# Create a header-only interface target so VS shows headers in the project
add_library(fastrules_headers INTERFACE)
set_property(TARGET fastrules_headers PROPERTY FOLDER "Core")
target_sources(fastrules_headers INTERFACE
    FILE_SET HEADERS
    BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include
    FILES ${FASTRULES_PUBLIC_HEADERS}
)

# Also add headers to the main library so they appear in VS Solution Explorer
target_sources(fastrules PRIVATE ${FASTRULES_PUBLIC_HEADERS})

# Organize headers in a "Header Files" folder in Visual Studio
source_group("Header Files" FILES ${FASTRULES_PUBLIC_HEADERS})

# Coverage instrumentation for MSVC
if(FASTRULES_BUILD_COVERAGE AND MSVC)
    target_compile_options(fastrules PRIVATE /Od /Zi)
    target_link_options(fastrules PRIVATE /DEBUG)
endif()

target_include_directories(fastrules
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(fastrules
    PUBLIC
        lua_static
        spdlog::spdlog
)

target_include_directories(fastrules PRIVATE ${luabridge3_SOURCE_DIR}/Source)

if(MSVC)
    target_compile_options(fastrules PRIVATE /W4 /WX-)
    if(FASTRULES_USE_LUAJIT)
        target_compile_definitions(fastrules PRIVATE FASTRULES_USE_LUAJIT)
    endif()
    target_compile_definitions(fastrules PUBLIC FASTRULES_USE_LUABRIDGE3)
else()
    target_compile_options(fastrules PRIVATE -Wall -Wextra -Wpedantic)
    if(FASTRULES_USE_LUAJIT)
        target_compile_definitions(fastrules PRIVATE FASTRULES_USE_LUAJIT)
    endif()
    target_compile_definitions(fastrules PUBLIC FASTRULES_USE_LUABRIDGE3)
endif()

# === Tests ===
if(FASTRULES_BUILD_TESTS)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.5.2
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(Catch2)

    add_executable(fastrules_tests
        tests/test_main.cpp
        tests/test_rule.cpp
        tests/test_workflow.cpp
        tests/test_lua_engine.cpp
        tests/test_async.cpp
        tests/test_security_features.cpp
        tests/test_new_features.cpp
        tests/test_execution_tracing.cpp
        tests/test_coverage_gaps.cpp
        tests/test_benchmarks.cpp
        tests/test_error_paths.cpp
        tests/test_fuzz_expression.cpp
    )

    set_property(TARGET fastrules_tests PROPERTY FOLDER "Tests")

    target_link_libraries(fastrules_tests PRIVATE fastrules Catch2::Catch2WithMain)

    # Coverage instrumentation for test target
    if(FASTRULES_BUILD_COVERAGE AND MSVC)
        target_compile_options(fastrules_tests PRIVATE /Od /Zi)
        target_link_options(fastrules_tests PRIVATE /DEBUG)
    endif()

    # Suppress CTest dashboard targets from VS solution
    set_property(GLOBAL PROPERTY CTEST_USE_LAUNCHERS OFF)

    include(Catch)
    catch_discover_tests(fastrules_tests)
endif()

# === Examples ===
if(FASTRULES_BUILD_EXAMPLES)
    add_executable(simple_example examples/simple_example.cpp)
    set_property(TARGET simple_example PROPERTY FOLDER "Examples")
    target_link_libraries(simple_example PRIVATE fastrules)

    add_executable(core_only_example examples/core_only_example.cpp)
    set_property(TARGET core_only_example PROPERTY FOLDER "Examples")
    target_link_libraries(core_only_example PRIVATE fastrules)

    add_executable(workflow_bubble_up examples/workflow_bubble_up.cpp)
    set_property(TARGET workflow_bubble_up PROPERTY FOLDER "Examples")
    target_link_libraries(workflow_bubble_up PRIVATE fastrules)

    add_executable(parent_child_example examples/parent_child_example.cpp)
    set_property(TARGET parent_child_example PROPERTY FOLDER "Examples")
    target_link_libraries(parent_child_example PRIVATE fastrules)

    add_executable(multi_object_example examples/multi_object_example.cpp)
    set_property(TARGET multi_object_example PROPERTY FOLDER "Examples")
    target_link_libraries(multi_object_example PRIVATE fastrules)

    add_executable(no_globals_example examples/no_globals_example.cpp)
    set_property(TARGET no_globals_example PROPERTY FOLDER "Examples")
    target_link_libraries(no_globals_example PRIVATE fastrules)

    add_executable(typed_example examples/typed_example.cpp)
    set_property(TARGET typed_example PROPERTY FOLDER "Examples")
    target_link_libraries(typed_example PRIVATE fastrules)

    add_executable(custom_methods_example examples/custom_methods_example.cpp)
    set_property(TARGET custom_methods_example PROPERTY FOLDER "Examples")
    target_link_libraries(custom_methods_example PRIVATE fastrules)

    add_executable(luabridge3_basic_example examples/luabridge3_basic_example.cpp)
    set_property(TARGET luabridge3_basic_example PROPERTY FOLDER "Examples")
    target_link_libraries(luabridge3_basic_example PRIVATE fastrules)

    add_executable(repl_example examples/repl_example.cpp)
    set_property(TARGET repl_example PROPERTY FOLDER "Examples")
    target_link_libraries(repl_example PRIVATE fastrules)

    add_executable(debug_context examples/debug_context.cpp)
    set_property(TARGET debug_context PROPERTY FOLDER "Examples")
    target_link_libraries(debug_context PRIVATE fastrules)
endif()

# JSON/XML-dependent examples (only if extensions are built)
if(FASTRULES_BUILD_EXAMPLES AND FASTRULES_BUILD_EXTENSIONS)
    add_executable(comprehensive_example examples/comprehensive_example.cpp)
    set_property(TARGET comprehensive_example PROPERTY FOLDER "Examples")
    target_link_libraries(comprehensive_example PRIVATE fastrules fastrules-json fastrules-xml)

    add_executable(loop_example examples/loop_example.cpp)
    set_property(TARGET loop_example PROPERTY FOLDER "Examples")
    target_link_libraries(loop_example PRIVATE fastrules fastrules-json)
endif()

# Optional extensions (JSON, XML, DB persistence)
if(FASTRULES_BUILD_EXTENSIONS)
    set(FASTRULES_BUILD_JSON_EXT ON CACHE BOOL "" FORCE)
    set(FASTRULES_BUILD_XML_EXT ON CACHE BOOL "" FORCE)
    if(FASTRULES_BUILD_DB)
        set(FASTRULES_BUILD_DB_EXT ON CACHE BOOL "" FORCE)
    else()
        set(FASTRULES_BUILD_DB_EXT OFF CACHE BOOL "" FORCE)
    endif()
    add_subdirectory(extensions)
endif()

# === Coverage target ===
if(FASTRULES_BUILD_COVERAGE AND MSVC)
    find_program(OpenCppCoverage_EXECUTABLE OpenCppCoverage
        PATHS 
            "C:/Program Files/OpenCppCoverage"
            "C:/Program Files (x86)/OpenCppCoverage"
            "$ENV{LOCALAPPDATA}/Programs/OpenCppCoverage"
    )
    
    if(OpenCppCoverage_EXECUTABLE)
        set(COVERAGE_DIR "${CMAKE_BINARY_DIR}/coverage")
        file(MAKE_DIRECTORY ${COVERAGE_DIR})
        
        add_custom_target(coverage
            COMMAND ${OpenCppCoverage_EXECUTABLE}
                --sources "${CMAKE_SOURCE_DIR}/include"
                --sources "${CMAKE_SOURCE_DIR}/src"
                --sources "${CMAKE_SOURCE_DIR}/extensions"
                --export_type html:${COVERAGE_DIR}/html
                --export_type cobertura:${COVERAGE_DIR}/cobertura.xml
                --working_dir "${CMAKE_BINARY_DIR}"
                -- ctest -C Debug --output-on-failure
            WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
            COMMENT "Generating code coverage report with OpenCppCoverage..."
            VERBATIM
        )
        
        message(STATUS "Coverage target 'coverage' created")
        message(STATUS "  Run: cmake --build . --target coverage")
    else()
        message(WARNING "OpenCppCoverage not found. Install with: choco install opencppcoverage")
    endif()
endif()

# === Install ===
include(GNUInstallDirs)

# Install the fastrules library (static only since deps are fetched)
install(TARGETS fastrules
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/fastrules
)

# Install headers
install(DIRECTORY include/fastrules
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Install third-party headers (fetched deps are part of the build)
if(EXISTS ${json_SOURCE_DIR}/single_include/nlohmann)
    install(DIRECTORY 
        ${json_SOURCE_DIR}/single_include/nlohmann
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )
endif()

if(EXISTS ${luabridge3_SOURCE_DIR}/Source/LuaBridge)
    install(DIRECTORY
        ${luabridge3_SOURCE_DIR}/Source/LuaBridge
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )
endif()

if(NOT FASTRULES_USE_LUAJIT)
    if(EXISTS ${lua_SOURCE_DIR})
        install(DIRECTORY
            ${lua_SOURCE_DIR}/
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/lua
            FILES_MATCHING PATTERN "*.h"
        )
    endif()
endif()

# Install Lua library (only if we built it from source, not imported)
if(NOT FASTRULES_USE_LUAJIT)
    install(TARGETS lua_static
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )
endif()

# Create package config files
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/fastrules-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/fastrules-config.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/fastrules-config.cmake"
    @ONLY
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/fastrules-config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/fastrules-config-version.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/fastrules
)

