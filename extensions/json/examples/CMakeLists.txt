cmake_minimum_required(VERSION 3.28)

# Individual examples for each JSON data file
add_executable(json_example_customer_rules example_customer_rules.cpp)
set_property(TARGET json_example_customer_rules PROPERTY FOLDER "Extensions/JSON/Examples")
target_link_libraries(json_example_customer_rules PRIVATE fastrules-json)

add_executable(json_example_customer_validation example_customer_validation.cpp)
set_property(TARGET json_example_customer_validation PROPERTY FOLDER "Extensions/JSON/Examples")
target_link_libraries(json_example_customer_validation PRIVATE fastrules-json)

add_executable(json_example_customer_validation_simple example_customer_validation_simple.cpp)
set_property(TARGET json_example_customer_validation_simple PROPERTY FOLDER "Extensions/JSON/Examples")
target_link_libraries(json_example_customer_validation_simple PRIVATE fastrules-json)

add_executable(json_example_dependency_rules example_dependency_rules.cpp)
set_property(TARGET json_example_dependency_rules PROPERTY FOLDER "Extensions/JSON/Examples")
target_link_libraries(json_example_dependency_rules PRIVATE fastrules-json)

add_executable(json_example_hierarchical_rules example_hierarchical_rules.cpp)
set_property(TARGET json_example_hierarchical_rules PROPERTY FOLDER "Extensions/JSON/Examples")
target_link_libraries(json_example_hierarchical_rules PRIVATE fastrules-json)

# Copy vcpkg DLLs and data files to output directory on Windows
if(WIN32 AND VCPKG_INSTALLED_DIR)
    set(JSON_EXAMPLES 
        json_example_customer_rules
        json_example_customer_validation
        json_example_customer_validation_simple
        json_example_dependency_rules
        json_example_hierarchical_rules
    )
    
    foreach(target ${JSON_EXAMPLES})
        # Copy vcpkg DLLs
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin"
                "$<TARGET_FILE_DIR:${target}>"
        )
        
        # Copy JSON data files
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy
                "${CMAKE_CURRENT_SOURCE_DIR}/customer_rules.json"
                "${CMAKE_CURRENT_SOURCE_DIR}/customer_validation.json"
                "${CMAKE_CURRENT_SOURCE_DIR}/customer_validation_simple.json"
                "${CMAKE_CURRENT_SOURCE_DIR}/dependency_rules.json"
                "${CMAKE_CURRENT_SOURCE_DIR}/hierarchical_rules.json"
                "$<TARGET_FILE_DIR:${target}>"
        )
    endforeach()
endif()
