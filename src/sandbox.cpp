#include "fastrules/sandbox.hpp"
#include "fastrules/logger.hpp"
#include "fastrules/input_validator.hpp"

#include <lua.hpp>
#include <algorithm>
#include <cctype>

namespace fastrules {

SandboxViolationException::SandboxViolationException(const std::string& message) 
    : std::runtime_error("Sandbox violation: " + message) {}

SandboxConfig::SandboxConfig()
    : maxMemoryBytes_(1024 * 1024 * 100)  // 100 MB default
    , maxInstructions_(1000000)           // 1 million instructions default
    , enabled_(true) {
    
    // Initialize restricted modules
    restrictedModules_ = {
        "os", "io", "debug", "package", "coroutine"
    };
    
    // Initialize restricted functions
    restrictedFunctions_ = {
        "dofile", "loadfile", "load", "loadstring",
        "require", "module", "setmetatable", "getmetatable",
        "rawget", "rawset", "rawequal", "collectgarbage",
        "gcinfo", "newproxy", "system", "exec", "spawn", "popen"
    };
    
    // Initialize allowed modules (whitelist approach)
    allowedModules_ = {
        "math", "string", "table", "bit32"
    };
    
    // Initialize allowed functions (whitelist approach)
    allowedFunctions_ = {
        "print", "type", "next", "ipairs", "pairs", "tonumber", "tostring"
    };
}

void SandboxConfig::setEnabled(bool enabled) {
    enabled_ = enabled;
}

bool SandboxConfig::isEnabled() const {
    return enabled_;
}

void SandboxConfig::addRestrictedModule(const std::string& module) {
    restrictedModules_.insert(module);
}

void SandboxConfig::removeRestrictedModule(const std::string& module) {
    restrictedModules_.erase(module);
}

bool SandboxConfig::isModuleRestricted(const std::string& module) const {
    return restrictedModules_.find(module) != restrictedModules_.end();
}

void SandboxConfig::addRestrictedFunction(const std::string& function) {
    restrictedFunctions_.insert(function);
}

void SandboxConfig::removeRestrictedFunction(const std::string& function) {
    restrictedFunctions_.erase(function);
}

bool SandboxConfig::isFunctionRestricted(const std::string& function) const {
    return restrictedFunctions_.find(function) != restrictedFunctions_.end();
}

void SandboxConfig::addAllowedModule(const std::string& module) {
    allowedModules_.insert(module);
}

void SandboxConfig::removeAllowedModule(const std::string& module) {
    allowedModules_.erase(module);
}

bool SandboxConfig::isModuleAllowed(const std::string& module) const {
    return allowedModules_.find(module) != allowedModules_.end();
}

void SandboxConfig::addAllowedFunction(const std::string& function) {
    allowedFunctions_.insert(function);
}

void SandboxConfig::removeAllowedFunction(const std::string& function) {
    allowedFunctions_.erase(function);
}

bool SandboxConfig::isFunctionAllowed(const std::string& function) const {
    return allowedFunctions_.find(function) != allowedFunctions_.end();
}

void SandboxConfig::setMaxMemory(size_t bytes) {
    maxMemoryBytes_ = bytes;
}

size_t SandboxConfig::getMaxMemory() const {
    return maxMemoryBytes_;
}

void SandboxConfig::setMaxInstructions(size_t instructions) {
    maxInstructions_ = instructions;
}

size_t SandboxConfig::getMaxInstructions() const {
    return maxInstructions_;
}

// Static instance for SandboxManager
static SandboxManager* g_sandboxManager = nullptr;

SandboxManager::SandboxManager()
    : config_(std::make_unique<SandboxConfig>()) {
}

SandboxConfig& SandboxManager::getConfig() {
    return *config_;
}

void SandboxManager::applySandbox(lua_State* lua) {
    if (!lua) {
        throw std::invalid_argument("Lua state is null");
    }
    
    if (!config_->isEnabled()) {
        return;  // Sandboxing is disabled
    }
    
    // Check if already sandboxed
    if (isSandboxed(lua)) {
        return;  // Already sandboxed
    }
    
    try {
        // Restrict dangerous functions
        restrictDangerousFunctions(lua);
        
        // Restrict dangerous modules
        restrictDangerousModules(lua);
        
        // Set memory limit
        setMemoryLimit(lua, config_->getMaxMemory());
        
        // Set instruction limit
        setInstructionLimit(lua, config_->getMaxInstructions());
        
        // Mark as sandboxed
        sandboxedStates_[lua] = true;
        
        auto log = logger();
        if (log) {
            log->info("Applied sandbox restrictions to Lua state");
        }
    } catch (const std::exception& e) {
        auto log = logger();
        if (log) {
            log->error("Failed to apply sandbox restrictions: {}", e.what());
        }
        throw SandboxViolationException("Failed to apply sandbox restrictions: " + std::string(e.what()));
    }
}

void SandboxManager::removeSandbox(lua_State* lua) {
    if (!lua) {
        return;
    }
    
    // Remove from sandboxed states
    sandboxedStates_.erase(lua);
    
    auto log = logger();
    if (log) {
        log->info("Removed sandbox restrictions from Lua state");
    }
}

bool SandboxManager::isSandboxed(lua_State* lua) const {
    if (!lua) {
        return false;
    }
    
    auto it = sandboxedStates_.find(lua);
    return it != sandboxedStates_.end() && it->second;
}

void SandboxManager::validateCode(const std::string& code) const {
    if (!config_->isEnabled()) {
        return;  // Sandboxing is disabled
    }
    
    // Use input validator to check for dangerous patterns
    try {
        auto& securityConfig = getSecurityConfig();
        auto& validator = securityConfig.getExpressionValidator();
        validator.validate(code);
    } catch (const ValidationException& e) {
        throw SandboxViolationException("Code validation failed: " + std::string(e.what()));
    }
}

void SandboxManager::restrictDangerousFunctions(lua_State* lua) {
    if (!lua) {
        return;
    }
    
    // Remove dangerous global functions
    for (const auto& function : config_->restrictedFunctions_) {
        lua_pushnil(lua);
        lua_setglobal(lua, function.c_str());
    }
    
    // Also check for functions in allowed modules that might be dangerous
    for (const auto& module : config_->allowedModules_) {
        // This is a simplified approach - in practice, you'd want to check
        // each function in each module against the restricted functions list
    }
}

void SandboxManager::restrictDangerousModules(lua_State* lua) {
    if (!lua) {
        return;
    }
    
    // Remove dangerous modules from global namespace
    for (const auto& module : config_->restrictedModules_) {
        lua_pushnil(lua);
        lua_setglobal(lua, module.c_str());
    }
}

void SandboxManager::setMemoryLimit(lua_State* lua, size_t maxBytes) {
    if (!lua) {
        return;
    }
    
    // In a real implementation, you would set up memory tracking
    // This is a placeholder for demonstration
    // Lua doesn't have built-in memory limits, so you'd need to implement
    // custom allocation functions or use platform-specific mechanisms
}

void SandboxManager::setInstructionLimit(lua_State* lua, size_t maxInstructions) {
    if (!lua) {
        return;
    }
    
    // Set instruction hook to limit execution
    // This is a simplified implementation
    lua_sethook(lua, [](lua_State* L, lua_Debug* ar) {
        // Get sandbox manager
        auto& manager = getSandboxManager();
        
        // Check if we've exceeded the instruction limit
        // This is a placeholder - in practice, you'd need to track instruction count
        static size_t instructionCount = 0;
        instructionCount++;
        
        auto& config = manager.getConfig();
        if (instructionCount > config.getMaxInstructions()) {
            luaL_error(L, "Instruction limit exceeded");
        }
    }, LUA_MASKCOUNT, 1000);  // Check every 1000 instructions
}

SandboxManager& getSandboxManager() {
    if (!g_sandboxManager) {
        g_sandboxManager = new SandboxManager();
    }
    return *g_sandboxManager;
}

SandboxGuard::SandboxGuard(lua_State* lua)
    : lua_(lua)
    , wasSandboxed_(false) {
    
    if (lua_) {
        auto& manager = getSandboxManager();
        wasSandboxed_ = manager.isSandboxed(lua_);
        if (!wasSandboxed_) {
            manager.applySandbox(lua_);
        }
    }
}

SandboxGuard::~SandboxGuard() {
    if (lua_ && !wasSandboxed_) {
        auto& manager = getSandboxManager();
        manager.removeSandbox(lua_);
    }
}

} // namespace fastrules