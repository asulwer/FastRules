# Install script for directory: C:/Users/asulw/.openclaw/workspace/main/fastrules

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "C:/Program Files (x86)/fastrules")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Release")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "C:/Users/asulw/.openclaw/workspace/main/fastrules/Debug/fastrules.lib")
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "C:/Users/asulw/.openclaw/workspace/main/fastrules/Release/fastrules.lib")
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "C:/Users/asulw/.openclaw/workspace/main/fastrules/MinSizeRel/fastrules.lib")
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "C:/Users/asulw/.openclaw/workspace/main/fastrules/RelWithDebInfo/fastrules.lib")
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/fastrules" TYPE FILE FILES
    "C:/Users/asulw/.openclaw/workspace/main/fastrules/include/fastrules/action_callback.hpp"
    "C:/Users/asulw/.openclaw/workspace/main/fastrules/include/fastrules/aot_compiler.hpp"
    "C:/Users/asulw/.openclaw/workspace/main/fastrules/include/fastrules/async_workflow.hpp"
    "C:/Users/asulw/.openclaw/workspace/main/fastrules/include/fastrules/execution_tracer.hpp"
    "C:/Users/asulw/.openclaw/workspace/main/fastrules/include/fastrules/expression_validator.hpp"
    "C:/Users/asulw/.openclaw/workspace/main/fastrules/include/fastrules/fastrules.hpp"
    "C:/Users/asulw/.openclaw/workspace/main/fastrules/include/fastrules/logger.hpp"
    "C:/Users/asulw/.openclaw/workspace/main/fastrules/include/fastrules/lua_backend.hpp"
    "C:/Users/asulw/.openclaw/workspace/main/fastrules/include/fastrules/lua_backend_luabridge.hpp"
    "C:/Users/asulw/.openclaw/workspace/main/fastrules/include/fastrules/lua_engine.hpp"
    "C:/Users/asulw/.openclaw/workspace/main/fastrules/include/fastrules/parameter_validator.hpp"
    "C:/Users/asulw/.openclaw/workspace/main/fastrules/include/fastrules/performance_counters.hpp"
    "C:/Users/asulw/.openclaw/workspace/main/fastrules/include/fastrules/rate_limiter.hpp"
    "C:/Users/asulw/.openclaw/workspace/main/fastrules/include/fastrules/rule.hpp"
    "C:/Users/asulw/.openclaw/workspace/main/fastrules/include/fastrules/rule_context.hpp"
    "C:/Users/asulw/.openclaw/workspace/main/fastrules/include/fastrules/rule_result.hpp"
    "C:/Users/asulw/.openclaw/workspace/main/fastrules/include/fastrules/rule_versioning.hpp"
    "C:/Users/asulw/.openclaw/workspace/main/fastrules/include/fastrules/streaming_result.hpp"
    "C:/Users/asulw/.openclaw/workspace/main/fastrules/include/fastrules/type_registry.hpp"
    "C:/Users/asulw/.openclaw/workspace/main/fastrules/include/fastrules/workflow.hpp"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include" TYPE DIRECTORY FILES "C:/Users/asulw/.openclaw/workspace/main/fastrules/include/fastrules")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/fastrules" TYPE FILE FILES
    "C:/Users/asulw/.openclaw/workspace/main/fastrules/fastrules-config.cmake"
    "C:/Users/asulw/.openclaw/workspace/main/fastrules/fastrules-config-version.cmake"
    )
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "C:/Users/asulw/.openclaw/workspace/main/fastrules/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
if(CMAKE_INSTALL_COMPONENT)
  if(CMAKE_INSTALL_COMPONENT MATCHES "^[a-zA-Z0-9_.+-]+$")
    set(CMAKE_INSTALL_MANIFEST "install_manifest_${CMAKE_INSTALL_COMPONENT}.txt")
  else()
    string(MD5 CMAKE_INST_COMP_HASH "${CMAKE_INSTALL_COMPONENT}")
    set(CMAKE_INSTALL_MANIFEST "install_manifest_${CMAKE_INST_COMP_HASH}.txt")
    unset(CMAKE_INST_COMP_HASH)
  endif()
else()
  set(CMAKE_INSTALL_MANIFEST "install_manifest.txt")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "C:/Users/asulw/.openclaw/workspace/main/fastrules/${CMAKE_INSTALL_MANIFEST}"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
