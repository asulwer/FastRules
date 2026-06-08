# CodeCoverage.cmake
# MSVC + OpenCppCoverage support for FastRules
#
# Usage:
#   cmake -DFASTRULES_BUILD_COVERAGE=ON ...
#   cmake --build . --config Debug
#   ctest -C Debug
#   cmake --build . --target coverage
#
# Requires: OpenCppCoverage (choco install opencppcoverage)

include(CMakeParseArguments)

option(FASTRULES_BUILD_COVERAGE "Build with coverage instrumentation" OFF)

if(FASTRULES_BUILD_COVERAGE)
    if(MSVC)
        # MSVC coverage requires /Od (disable optimization) for accurate line coverage
        add_compile_options(/Od /Zi)
        add_link_options(/DEBUG)
        
        # Check for OpenCppCoverage
        find_program(OpenCppCoverage_EXECUTABLE OpenCppCoverage
            PATHS 
                "C:/Program Files/OpenCppCoverage"
                "C:/Program Files (x86)/OpenCppCoverage"
                "$ENV{LOCALAPPDATA}/Programs/OpenCppCoverage"
        )
        
        if(NOT OpenCppCoverage_EXECUTABLE)
            message(WARNING "OpenCppCoverage not found. Install with: choco install opencppcoverage")
            message(STATUS "Coverage reports will not be generated.")
        else()
            message(STATUS "OpenCppCoverage found: ${OpenCppCoverage_EXECUTABLE}")
        endif()
    else()
        # GCC/Clang - use gcov
        add_compile_options(--coverage -O0 -g)
        add_link_options(--coverage)
    endif()
endif()

function(add_coverage_target TARGET_NAME)
    if(NOT FASTRULES_BUILD_COVERAGE)
        return()
    endif()
    
    if(MSVC AND OpenCppCoverage_EXECUTABLE)
        # Collect all test executables
        get_property(test_targets DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY TEST_TARGETS)
        
        # Build source patterns for all include and src directories
        set(SOURCE_PATTERNS "")
        get_target_property(target_sources ${TARGET_NAME} SOURCES)
        
        # Create coverage report directory
        set(COVERAGE_DIR "${CMAKE_BINARY_DIR}/coverage")
        file(MAKE_DIRECTORY ${COVERAGE_DIR})
        
        # Generate coverage target
        add_custom_target(${TARGET_NAME}_coverage
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
        
        # Also create a merged target if it doesn't exist
        if(NOT TARGET coverage)
            add_custom_target(coverage
                COMMENT "Run all coverage analysis"
            )
        endif()
        add_dependencies(coverage ${TARGET_NAME}_coverage)
        
        message(STATUS "Coverage target '${TARGET_NAME}_coverage' created")
        message(STATUS "  Run: cmake --build . --target ${TARGET_NAME}_coverage")
        message(STATUS "  Or: cmake --build . --target coverage")
    elseif(NOT MSVC)
        # GCC/Clang gcov target
        find_program(GCOV_EXECUTABLE gcov)
        find_program(LCOV_EXECUTABLE lcov)
        find_program(GENHTML_EXECUTABLE genhtml)
        
        if(LCOV_EXECUTABLE AND GENHTML_EXECUTABLE)
            set(COVERAGE_DIR "${CMAKE_BINARY_DIR}/coverage")
            
            add_custom_target(${TARGET_NAME}_coverage
                COMMAND ${LCOV_EXECUTABLE} --capture --directory . --output-file ${COVERAGE_DIR}/coverage.info
                COMMAND ${GENHTML_EXECUTABLE} ${COVERAGE_DIR}/coverage.info --output-directory ${COVERAGE_DIR}/html
                WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
                COMMENT "Generating lcov coverage report..."
            )
            
            if(NOT TARGET coverage)
                add_custom_target(coverage)
            endif()
            add_dependencies(coverage ${TARGET_NAME}_coverage)
        endif()
    endif()
endfunction()
