cmake_minimum_required(VERSION 3.28)
project(fastrules VERSION 0.1.0 LANGUAGES C CXX)

# CMP0091: MSVC runtime library flags are selected by the MSVC_RUNTIME_LIBRARY target property
# Must be set before any targets are created
cmake_policy(SET CMP0091 NEW)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler warnings - optional, don't fail if not supported
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    # GCC/Clang warnings - add only if supported
    include(CheckCXXCompilerFlag)
    foreach(WARNING_FLAG -Wall -Wextra -Wshadow -Wnon-virtual-dtor -Wunused -Woverloaded-virtual)
        string(REPLACE "-" "_" FLAG_VAR "CXX_SUPPORTS_${WARNING_FLAG}")
        check_cxx_compiler_flag(${WARNING_FLAG} ${FLAG_VAR})
        if(${FLAG_VAR})
            add_compile_options(${WARNING_FLAG})
        endif()
    endforeach()
    # Enable address sanitizer in debug builds
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        option(ENABLE_ASAN "Enable Address Sanitizer" OFF)
        if(ENABLE_ASAN)
            add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
            add_link_options(-fsanitize=address)
        endif()
    endif()
elseif(MSVC)
    # MSVC warnings: level 4, warnings on, not fatal (/WX-). Do not add
    # project-wide /wd flags here; fix the code that causes warnings instead.
    add_compile_options(
        /W4
        /WX-
    )
endif()

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

# Ensure our lua.hpp wrapper is found before any fetched Lua sources.
# PUC-Rio Lua doesn't ship lua.hpp, but LuaBridge3 requires it.
include_directories(BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Define options BEFORE collecting vcpkg features
option(FASTRULES_BUILD_TESTS "Build tests" ON)
option(FASTRULES_BUILD_EXAMPLES "Build examples" ON)
option(FASTRULES_BUILD_STRESS_TESTS "Build stress tests" OFF)
option(FASTRULES_BUILD_EXTENSIONS "Build optional persistence extensions" OFF)
option(FASTRULES_BUILD_COVERAGE "Build with coverage instrumentation" OFF)
option(FASTRULES_BUILD_SHARED "Build as shared library (DLL/SO/Dylib)" OFF)
# LuaBridge3 backend only
set(FASTRULES_LUA_BACKEND "luabridge3" CACHE STRING "Lua binding backend")

# When extensions are enabled, all three are built together.
if(FASTRULES_BUILD_EXTENSIONS)
    set(FASTRULES_BUILD_JSON_EXT ON)
    set(FASTRULES_BUILD_XML_EXT ON)
    set(FASTRULES_BUILD_DB_EXT ON)
else()
    set(FASTRULES_BUILD_JSON_EXT OFF)
    set(FASTRULES_BUILD_XML_EXT OFF)
    set(FASTRULES_BUILD_DB_EXT OFF)
endif()

# Collect requested vcpkg features based on build options.
# NOTE: VCPKG_MANIFEST_FEATURES must be set as a CMake cache variable (e.g. via
# -DVCPKG_MANIFEST_FEATURES=...) because vcpkg evaluates it before CMakeLists.txt runs.
set(_FASTRULES_VCPKG_FEATURES "")
if(FASTRULES_BUILD_TESTS)
    list(APPEND _FASTRULES_VCPKG_FEATURES "tests")
endif()
if(FASTRULES_BUILD_EXTENSIONS)
    list(APPEND _FASTRULES_VCPKG_FEATURES "json" "xml")
    if(FASTRULES_BUILD_DB_EXT)
        list(APPEND _FASTRULES_VCPKG_FEATURES "db")
    endif()
endif()
if(_FASTRULES_VCPKG_FEATURES)
    message(STATUS "Requested vcpkg manifest features: ${_FASTRULES_VCPKG_FEATURES}")
endif()

# Suppress CTest dashboard targets from all dependencies
set_property(GLOBAL PROPERTY CTEST_TARGETS_ADDED 1)

# Third-party dependencies: vcpkg provides everything except luabridge3,
# which is header-only and not in the vcpkg registry.
include(FetchContent)

FetchContent_Declare(
    luabridge3
    GIT_REPOSITORY https://github.com/kunitoki/LuaBridge3.git
    GIT_TAG master
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
)

# Find vcpkg packages
find_package(spdlog CONFIG REQUIRED)
find_package(unofficial-lua CONFIG REQUIRED)
find_package(doctest CONFIG REQUIRED)

# Lua wrapper target: single canonical target regardless of platform or build mode
add_library(lua_static INTERFACE)
target_link_libraries(lua_static INTERFACE unofficial::lua::lua)

# Fetch LuaBridge3 (header-only, not available via vcpkg)
FetchContent_MakeAvailable(luabridge3)
if(TARGET luabridge3)
    set_target_properties(luabridge3 PROPERTIES FOLDER "ThirdParty")
endif()
if(TARGET Documentation)
    set_target_properties(Documentation PROPERTIES FOLDER "ThirdParty")
    set_property(TARGET Documentation PROPERTY EXCLUDE_FROM_ALL TRUE)
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
    src/work_stealing_thread_pool.cpp
    src/memory_pool.cpp
    src/input_validator.cpp
    src/timeout_executor.cpp
    src/sandbox.cpp
)

# Backend-specific sources
list(APPEND FASTRULES_CORE_SOURCES src/lua_backend_factory.cpp src/lua_backend_luabridge.cpp)

# C API sources (always included - part of core)
list(APPEND FASTRULES_CORE_SOURCES src/c_api.cpp)

# Collect public headers so they appear in Visual Studio projects
set(FASTRULES_PUBLIC_HEADERS
    include/fastrules/action_callback.hpp
    include/fastrules/aot_compiler.hpp
    include/fastrules/async_workflow.hpp
    include/fastrules/fastrules.h
    include/fastrules/execution_tracer.hpp
    include/fastrules/expression_validator.hpp
    include/fastrules/fastrules.hpp
    include/fastrules/fastrules_export.hpp
    include/fastrules/engine_pool.hpp
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
    include/fastrules/simple_work_stealing_queue.hpp  # Work-stealing queue
    include/fastrules/aot_compiler.hpp  # AOT compilation
    include/fastrules/memory_pool.hpp  # Memory pooling
    include/fastrules/input_validator.hpp  # Input validation
    include/fastrules/timeout_executor.hpp  # Timeout enforcement
    include/fastrules/sandbox.hpp  # Lua sandboxing
    include/fastrules/streaming_result.hpp
    include/fastrules/type_registry.hpp
    include/fastrules/workflow.hpp
    include/fastrules/work_stealing_thread_pool.hpp  # Work-stealing thread pool
)

# ============================================================================
# fastrules library
# ============================================================================
if(FASTRULES_BUILD_SHARED)
    add_library(fastrules SHARED ${FASTRULES_CORE_SOURCES})
    target_compile_definitions(fastrules PRIVATE FASTRULES_BUILDING_SHARED)
    set_target_properties(fastrules PROPERTIES
        WINDOWS_EXPORT_ALL_SYMBOLS TRUE  # Export all symbols on Windows
    )
else()
    add_library(fastrules STATIC ${FASTRULES_CORE_SOURCES})
    target_compile_definitions(fastrules PUBLIC FASTRULES_STATIC)
endif()
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

# C API is part of the core library; export it when building shared
target_include_directories(fastrules PRIVATE ${luabridge3_SOURCE_DIR}/Source)

target_compile_definitions(fastrules PUBLIC FASTRULES_USE_LUABRIDGE3)

# C API is part of the core library; export symbols with the same rules as the
# rest of the library via fastrules_export.hpp / fastrules.h.

# === Tests ===
if(FASTRULES_BUILD_TESTS)
    enable_testing()

    # Pass CTest integration through to extension subdirectories
    set(FASTRULES_IN_TREE_TESTS ON)


    # doctest is provided by vcpkg
    find_package(doctest CONFIG REQUIRED)

    file(GLOB FASTRULES_TEST_SOURCES tests/*.cpp)
    add_executable(fastrules_tests ${FASTRULES_TEST_SOURCES})

    set_property(TARGET fastrules_tests PROPERTY FOLDER "Tests")

    target_link_libraries(fastrules_tests PRIVATE fastrules doctest::doctest)

    # Coverage instrumentation for test target
    if(FASTRULES_BUILD_COVERAGE AND MSVC)
        target_compile_options(fastrules_tests PRIVATE /Od /Zi)
        target_link_options(fastrules_tests PRIVATE /DEBUG)
    endif()

    # Suppress CTest dashboard targets from VS solution
    set_property(GLOBAL PROPERTY CTEST_USE_LAUNCHERS OFF)

    # On Windows, copy runtime DLLs next to test executables so ctest can run
    # them without relying on PATH. Only vcpkg DLLs are copied here; the main
    # fastrules.dll is already next to fastrules_tests in the root output dir.
    if(WIN32)
        # Only copy DLLs matching the current build configuration to avoid
        # mixing Debug/Release runtimes (e.g. soci_sqlite3_4_0.dll has no d-suffix).
        set(_FASTRULES_TEST_DLL_SRC_DIR
            "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/$<$<CONFIG:Debug>:debug/>bin"
        )

        add_custom_command(TARGET fastrules_tests POST_BUILD
            COMMAND ${CMAKE_COMMAND}
                -D SRC_DIRS="${_FASTRULES_TEST_DLL_SRC_DIR}"
                -D DST_DIR="$<TARGET_FILE_DIR:fastrules_tests>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/copy_test_dlls.cmake"
            COMMENT "Copying runtime DLLs to fastrules_tests output directory"
        )
    endif()

    add_test(NAME fastrules_tests COMMAND fastrules_tests)
endif()

# === Stress tests ===
if(FASTRULES_BUILD_STRESS_TESTS)
    set(FASTRULES_IN_TREE_STRESS ON)
    add_subdirectory(stress)
endif()

# === Examples ===
if(FASTRULES_BUILD_EXAMPLES)
    set(FASTRULES_EXAMPLES
        simple_example
        luabridge3_basic_example
        core_only_example
        workflow_bubble_up
        parent_child_example
        multi_object_example
        no_globals_example
        typed_example
        custom_methods_example
        repl_example
        coroutine_example
        aot_compiler_example
        memory_pool_example
        input_validator_example
        timeout_executor_example
        sandbox_example
        debug_context
        macro_example
        console_logging_example
        loop_example
        performance_test
    )

    foreach(example ${FASTRULES_EXAMPLES})
        add_executable(${example} examples/${example}.cpp)
        set_property(TARGET ${example} PROPERTY FOLDER "Examples")
        target_link_libraries(${example} PRIVATE fastrules)
    endforeach()

    # Copy DLL to FFI example directories when building shared library on Windows
    if(FASTRULES_BUILD_SHARED AND WIN32)
        file(MAKE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/examples/csharp_example")
        file(MAKE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/examples/python_example")

        add_custom_command(TARGET fastrules POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:fastrules>"
                "${CMAKE_CURRENT_SOURCE_DIR}/examples/csharp_example/"
            COMMENT "Copying fastrules.dll to csharp_example"
        )
        add_custom_command(TARGET fastrules POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:fastrules>"
                "${CMAKE_CURRENT_SOURCE_DIR}/examples/python_example/"
            COMMENT "Copying fastrules.dll to python_example"
        )

        # Custom targets to run C# and Python examples from the IDE
        add_custom_target(csharp_example
            COMMAND dotnet run --project "${CMAKE_CURRENT_SOURCE_DIR}/examples/csharp_example/FastRulesExample.csproj"
            DEPENDS fastrules
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/examples/csharp_example"
            COMMENT "Running C# example"
            SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/examples/csharp_example/FastRulesExample.cs"
        )
        set_property(TARGET csharp_example PROPERTY FOLDER "Examples")

        add_custom_target(python_example
            COMMAND python "${CMAKE_CURRENT_SOURCE_DIR}/examples/python_example/fastrules_example.py"
            DEPENDS fastrules
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/examples/python_example"
            COMMENT "Running Python example"
            SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/examples/python_example/fastrules_example.py"
        )
        set_property(TARGET python_example PROPERTY FOLDER "Examples")
    endif()
endif()

# Optional extensions (JSON, XML, DB persistence)
if(FASTRULES_BUILD_EXTENSIONS)
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

# Install Lua library
install(TARGETS lua_static
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

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
