/**
 * @file lua_backend_factory.cpp
 * @brief Lua backend factory implementation
 * 
 * This file implements the factory function for creating Lua backend
 * instances. Currently only LuaBridge3Backend is supported.
 * 
 * Backend Selection:
 * - Currently hardcoded to LuaBridge3Backend
 * - Future: Could select based on configuration or availability
 * - LuaBridge3Backend uses sol2 for C++/Lua binding
 * 
 * Factory Pattern:
 * - LuaEngine uses backend abstraction
 * - Factory decouples engine from specific backend
 * - Easy to swap or add new backends (e.g., raw lua, luajit)
 * 
 * Thread Safety:
 * - Factory function is thread-safe
 * - Each call creates independent backend instance
 * - No shared state between backends
 */

#include "fastrules/lua_backend.hpp"
#include "fastrules/lua_backend_luabridge.hpp"

namespace fastrules {

/**
 * @brief Factory function to create a Lua backend instance
 * 
 * Creates and returns a unique_ptr to a new LuaBridge3Backend.
 * The caller owns the returned backend.
 * 
 * @return Unique pointer to newly created LuaBackend
 */
std::unique_ptr<LuaBackend> LuaBackend::create() {
    return std::make_unique<LuaBridge3Backend>();
}

} // namespace fastrules
