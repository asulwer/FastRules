#include "fastrules/async_registry.hpp"

#ifdef FASTRULES_USE_SOL2
#include <stdexcept>

namespace fastrules {

void AsyncRegistry::registerFunction(const std::string& name, AsyncHandler handler) {
    handlers_[name] = std::move(handler);
}

void AsyncRegistry::bindToLua(sol::state& lua) {
    // Create a table to hold async functions
    sol::table asyncTable = lua.create_table();
    
    for (const auto& [name, handler] : handlers_) {
        asyncTable[name] = [&lua, name, handler, this](sol::variadic_args args) -> sol::object {
            // Collect arguments
            std::vector<sol::object> argList;
            for (auto arg : args) {
                argList.push_back(arg);
            }
            
            // Create a promise that Lua can await
            // For now, we return a placeholder. In a full implementation,
            // this would integrate with Lua coroutines.
            sol::table promise = lua.create_table();
            promise["name"] = name;
            promise["args"] = argList;
            promise["pending"] = true;
            
            // Store the handler for later resolution
            // In a real implementation, this would be resolved by the caller
            
            return promise;
        };
    }
    
    lua["async"] = asyncTable;
}

bool AsyncRegistry::hasHandler(const std::string& name) const {
    return handlers_.find(name) != handlers_.end();
}

std::vector<std::string> AsyncRegistry::getHandlerNames() const {
    std::vector<std::string> names;
    for (const auto& [name, _] : handlers_) {
        names.push_back(name);
    }
    return names;
}

} // namespace fastrules
#endif // FASTRULES_USE_SOL2
