cmake_minimum_required(VERSION 3.28)

# Individual examples for each XML data file
add_executable(xml_example_customer_rules example_customer_rules.cpp)
set_property(TARGET xml_example_customer_rules PROPERTY FOLDER "Extensions/XML/Examples")
target_link_libraries(xml_example_customer_rules PRIVATE fastrules-xml)

add_executable(xml_example_customer_validation example_customer_validation.cpp)
set_property(TARGET xml_example_customer_validation PROPERTY FOLDER "Extensions/XML/Examples")
target_link_libraries(xml_example_customer_validation PRIVATE fastrules-xml)

add_executable(xml_example_customer_validation_simple example_customer_validation_simple.cpp)
set_property(TARGET xml_example_customer_validation_simple PROPERTY FOLDER "Extensions/XML/Examples")
target_link_libraries(xml_example_customer_validation_simple PRIVATE fastrules-xml)

add_executable(xml_example_dependency_rules example_dependency_rules.cpp)
set_property(TARGET xml_example_dependency_rules PROPERTY FOLDER "Extensions/XML/Examples")
target_link_libraries(xml_example_dependency_rules PRIVATE fastrules-xml)

add_executable(xml_example_hierarchical_rules example_hierarchical_rules.cpp)
set_property(TARGET xml_example_hierarchical_rules PROPERTY FOLDER "Extensions/XML/Examples")
target_link_libraries(xml_example_hierarchical_rules PRIVATE fastrules-xml)

# Copy vcpkg DLLs and data files to output directory on Windows
if(WIN32 AND VCPKG_INSTALLED_DIR)
    set(XML_EXAMPLES 
        xml_example_customer_rules
        xml_example_customer_validation
        xml_example_customer_validation_simple
        xml_example_dependency_rules
        xml_example_hierarchical_rules
    )
    
    foreach(target ${XML_EXAMPLES})
        # Copy vcpkg DLLs
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/$<$<CONFIG:Debug>:debug/>bin"
                "$<TARGET_FILE_DIR:${target}>"
        )
        
        # Copy XML data files
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy
                "${CMAKE_CURRENT_SOURCE_DIR}/customer_rules.xml"
                "${CMAKE_CURRENT_SOURCE_DIR}/customer_validation.xml"
                "${CMAKE_CURRENT_SOURCE_DIR}/customer_validation_simple.xml"
                "${CMAKE_CURRENT_SOURCE_DIR}/dependency_rules.xml"
                "${CMAKE_CURRENT_SOURCE_DIR}/hierarchical_rules.xml"
                "$<TARGET_FILE_DIR:${target}>"
        )
    endforeach()
endif()
