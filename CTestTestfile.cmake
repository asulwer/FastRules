# CMake generated Testfile for 
# Source directory: C:/Users/asulw/.openclaw/workspace/main/fastrules
# Build directory: C:/Users/asulw/.openclaw/workspace/main/fastrules
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
if(CTEST_CONFIGURATION_TYPE MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
  add_test([=[fastrules_tests]=] "C:/Users/asulw/.openclaw/workspace/main/fastrules/Debug/fastrules_tests.exe")
  set_tests_properties([=[fastrules_tests]=] PROPERTIES  _BACKTRACE_TRIPLES "C:/Users/asulw/.openclaw/workspace/main/fastrules/CMakeLists.txt;463;add_test;C:/Users/asulw/.openclaw/workspace/main/fastrules/CMakeLists.txt;0;")
elseif(CTEST_CONFIGURATION_TYPE MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
  add_test([=[fastrules_tests]=] "C:/Users/asulw/.openclaw/workspace/main/fastrules/Release/fastrules_tests.exe")
  set_tests_properties([=[fastrules_tests]=] PROPERTIES  _BACKTRACE_TRIPLES "C:/Users/asulw/.openclaw/workspace/main/fastrules/CMakeLists.txt;463;add_test;C:/Users/asulw/.openclaw/workspace/main/fastrules/CMakeLists.txt;0;")
elseif(CTEST_CONFIGURATION_TYPE MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
  add_test([=[fastrules_tests]=] "C:/Users/asulw/.openclaw/workspace/main/fastrules/MinSizeRel/fastrules_tests.exe")
  set_tests_properties([=[fastrules_tests]=] PROPERTIES  _BACKTRACE_TRIPLES "C:/Users/asulw/.openclaw/workspace/main/fastrules/CMakeLists.txt;463;add_test;C:/Users/asulw/.openclaw/workspace/main/fastrules/CMakeLists.txt;0;")
elseif(CTEST_CONFIGURATION_TYPE MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
  add_test([=[fastrules_tests]=] "C:/Users/asulw/.openclaw/workspace/main/fastrules/RelWithDebInfo/fastrules_tests.exe")
  set_tests_properties([=[fastrules_tests]=] PROPERTIES  _BACKTRACE_TRIPLES "C:/Users/asulw/.openclaw/workspace/main/fastrules/CMakeLists.txt;463;add_test;C:/Users/asulw/.openclaw/workspace/main/fastrules/CMakeLists.txt;0;")
else()
  add_test([=[fastrules_tests]=] NOT_AVAILABLE)
endif()
