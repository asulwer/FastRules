# CMakeLists.txt for FastRules Examples
# Builds C API wrapper (DLL) for Python and C# FFI

cmake_minimum_required(VERSION 3.20)
project(fastrules_examples VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find FastRules package (from parent build)
find_package(fastrules CONFIG REQUIRED)

# ============================================================================
# C API Wrapper Library (for Python/C# FFI)
# ============================================================================
add_library(fastrules_c_api SHARED
    python_example/fastrules_c_api.cpp
)

target_include_directories(fastrules_c_api PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/python_example
)

target_link_libraries(fastrules_c_api PUBLIC
    fastrules::fastrules
)

# Export all symbols for C API
target_compile_definitions(fastrules_c_api PRIVATE
    FASTRULES_C_API_BUILDING
)

if(WIN32)
    set_target_properties(fastrules_c_api PROPERTIES
        WINDOWS_EXPORT_ALL_SYMBOLS TRUE
    )
endif()

set_target_properties(fastrules_c_api PROPERTIES
    OUTPUT_NAME "fastrules_c_api"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

# ============================================================================
# Copy DLLs to output directory for convenience
# ============================================================================
add_custom_command(TARGET fastrules_c_api POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:fastrules::fastrules>
        $<TARGET_FILE_DIR:fastrules_c_api>
)

# ============================================================================
# C# Example Executable (optional - requires .NET SDK)
# ============================================================================
find_program(DOTNET_EXECUTABLE dotnet)
if(DOTNET_EXECUTABLE)
    add_custom_target(csharp_example
        COMMAND ${DOTNET_EXECUTABLE} build
            ${CMAKE_CURRENT_SOURCE_DIR}/csharp_example/FastRulesExample.csproj
            -c Release
            -o ${CMAKE_BINARY_DIR}/csharp_output
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/csharp_example
        COMMENT "Building C# example"
    )
    add_dependencies(csharp_example fastrules_c_api)
endif()

# ============================================================================
# Python Example Test (optional - requires Python)
# ============================================================================
find_package(Python3 COMPONENTS Interpreter)
if(Python3_FOUND)
    add_custom_target(python_example
        COMMAND ${CMAKE_COMMAND} -E echo "Python example ready at: ${CMAKE_CURRENT_SOURCE_DIR}/python_example/fastrules_example.py"
        COMMAND ${CMAKE_COMMAND} -E echo "To run: python fastrules_example.py"
        COMMAND ${CMAKE_COMMAND} -E echo "(Make sure fastrules_c_api.dll is in PATH)"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/python_example
        COMMENT "Python example info"
    )
    add_dependencies(python_example fastrules_c_api)
endif()

message(STATUS "FastRules examples configuration complete")
message(STATUS "  - C API wrapper: fastrules_c_api")
message(STATUS "  - Python example: ${CMAKE_CURRENT_SOURCE_DIR}/python_example")
message(STATUS "  - C# example: ${CMAKE_CURRENT_SOURCE_DIR}/csharp_example")
