#include "fastrules/action_callback.hpp"

#ifdef FASTRULES_USE_SOL2
#include <iostream>

namespace fastrules {

void ActionCallbacks::registerStub(const std::string& name) {
    if (handlers_.contains(name)) {
        return; // Already registered
    }
    
    // Register a stub that warns when called
    handlers_[name] = [name](sol::object /*target*/, const std::vector<sol::object>& /*args*/) {
        std::cerr << "[fastrules] WARNING: Action callback '" << name 
                  << "' was called but no handler was registered."
                  << " Use engine.registerAction(\"" << name << "\", ...) to provide a handler."
                  << std::endl;
    };
}

void ActionCallbacks::bindToLua(sol::state& lua) {
    // Create a "callbacks" table in Lua
    sol::table callbacksTable = lua.create_table();
    
    for (const auto& [name, handler] : handlers_) {
        // Each callback gets a wrapper function in Lua
        // Signature: callbacks.name(target, ...)
        callbacksTable.set_function(name, [this, name](sol::object target, sol::variadic_args args) {
            // Convert variadic args to vector (skip target which is the first arg)
            std::vector<sol::object> argVec;
            for (auto arg : args) {
                argVec.push_back(arg);
            }
            
            auto it = handlers_.find(name);
            if (it != handlers_.end()) {
                it->second(target, argVec);
            }
        });
    }
    
    lua.set("callbacks", callbacksTable);
}

} // namespace fastrules
#endif // FASTRULES_USE_SOL2
