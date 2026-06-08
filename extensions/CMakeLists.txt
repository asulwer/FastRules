# Optional extensions for FastRules persistence
# Each extension is independent — only build what you need

cmake_minimum_required(VERSION 3.28)

# Build all extensions that have their dependencies available
option(FASTRULES_BUILD_JSON_EXT "Build JSON extension (loader + persistence)" ON)
option(FASTRULES_BUILD_XML_EXT "Build XML persistence extension" ON)
option(FASTRULES_BUILD_DB_EXT "Build database persistence extension" ON)

if(FASTRULES_BUILD_JSON_EXT)
    add_subdirectory(json)
endif()

if(FASTRULES_BUILD_XML_EXT)
    add_subdirectory(xml)
endif()

if(FASTRULES_BUILD_DB_EXT)
    add_subdirectory(db)
endif()
