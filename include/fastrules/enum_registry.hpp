#pragma once

#include <sol/sol.hpp>
#include <string>
#include <map>

namespace fastrules {

// ============================================================================
// Enum Registration - Maps C++ enums to Lua tables
// ============================================================================

// Helper to register an enum with Lua
// Usage: registerEnum<MyEnum>(engine.state(), "MyEnum", {{MyEnum::Value1, "Value1"}, {MyEnum::Value2, "Value2"}});
template<typename EnumType>
void registerEnum(sol::state& lua, const std::string& name, const std::map<EnumType, std::string>& values) {
    sol::table enumTable = lua.create_table();
    
    // Forward mapping: enum value -> name
    for (const auto& [value, valueName] : values) {
        enumTable[valueName] = static_cast<int>(value);
    }
    
    // Reverse mapping: name -> enum value (for parsing)
    sol::table reverseTable = lua.create_table();
    for (const auto& [value, valueName] : values) {
        reverseTable[static_cast<int>(value)] = valueName;
    }
    enumTable["_reverse"] = reverseTable;
    
    lua[name] = enumTable;
}

// Simplified macro for registering enums
// Usage: 
//   enum class Status { Active, Inactive, Pending };
//   REGISTER_ENUM(engine.state(), Status, Active, Inactive, Pending);
#define REGISTER_ENUM(lua_state, enum_name, ...) \
    do { \
        sol::state& _lua_ref = (lua_state); \
        sol::table _enum_table = _lua_ref.create_table(); \
        int _enum_values[] = {__VA_ARGS__}; \
        const char* _enum_names[] = {#__VA_ARGS__}; \
        for (size_t _i = 0; _i < sizeof(_enum_values)/sizeof(_enum_values[0]); ++_i) { \
            _enum_table[_enum_names[_i]] = _enum_values[_i]; \
        } \
        _lua_ref[#enum_name] = _enum_table; \
    } while(0)

} // namespace fastrules
