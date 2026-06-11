

# FastRules CMake package configuration
# 
# Dependencies:
#   - Lua (5.4 or LuaJIT) — required
#   - spdlog — required
#   - nlohmann_json — optional (JSON extension)
#   - pugixml — optional (XML extension)
#   - SOCI — optional (DB extension)

include(CMakeFindDependencyMacro)

find_dependency(lua REQUIRED)
find_dependency(spdlog REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/fastrules-targets.cmake")

check_required_components(fastrules)
