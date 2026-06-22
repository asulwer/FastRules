#include "fastrules/sandbox.hpp"
#include "fastrules/logger.hpp"
#include "fastrules/input_validator.hpp"

#include <lua.hpp>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <regex>

namespace fastrules {

// ============================================================================
// Per-state enforcement data + Lua hooks/allocator
// ============================================================================

// Granularity (in VM instructions) at which the count hook fires.
static constexpr int kInstructionHookStep = 1000;

// Unique registry key (its address) under which we stash the SandboxStateData
// pointer for the running Lua state, so the instruction hook can find it.
static const char kSandboxDataKey = 0;

struct SandboxStateData {
    // Instruction limiting
    size_t instructionCount = 0;
    size_t instructionLimit = 0;   // 0 == unlimited

    // Memory limiting (tracks allocations routed through limitAlloc)
    size_t memUsed = 0;
    size_t memLimit = 0;           // 0 == unlimited

    // Saved original allocator so it can be restored on removeSandbox
    lua_Alloc origAlloc = nullptr;
    void* origUd = nullptr;
    bool allocInstalled = false;
};

// Memory-capping allocator. Denies (returns nullptr) any growth that would push
// tracked usage past the configured limit; Lua turns that into a memory error.
static void* sandboxLimitAlloc(void* ud, void* ptr, size_t osize, size_t nsize) {
    auto* d = static_cast<SandboxStateData*>(ud);
    // When ptr is null, osize encodes the object kind, not a real size.
    size_t freed = (ptr != nullptr) ? osize : 0;

    if (nsize == 0) {
        std::free(ptr);
        if (d) d->memUsed -= (freed <= d->memUsed) ? freed : d->memUsed;
        return nullptr;
    }

    if (d && d->memLimit != 0) {
        size_t projected = d->memUsed - ((freed <= d->memUsed) ? freed : d->memUsed) + nsize;
        if (projected > d->memLimit) {
            return nullptr;  // deny: triggers Lua out-of-memory handling
        }
    }

    void* np = std::realloc(ptr, nsize);
    if (np && d) {
        d->memUsed = d->memUsed - ((freed <= d->memUsed) ? freed : d->memUsed) + nsize;
    }
    return np;
}

// Instruction-count hook. Fetches the SandboxStateData pointer from the registry
// and raises a Lua error once the configured instruction budget is exceeded.
static void sandboxInstructionHook(lua_State* L, lua_Debug* /*ar*/) {
    lua_pushlightuserdata(L, const_cast<char*>(&kSandboxDataKey));
    lua_rawget(L, LUA_REGISTRYINDEX);
    auto* d = static_cast<SandboxStateData*>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    if (!d || d->instructionLimit == 0) {
        return;
    }
    d->instructionCount += static_cast<size_t>(kInstructionHookStep);
    if (d->instructionCount > d->instructionLimit) {
        luaL_error(L, "Sandbox instruction limit exceeded (%d)",
                   static_cast<int>(d->instructionLimit));
    }
}

SandboxConfig::SandboxConfig()
    : maxMemoryBytes_(1024 * 1024 * 100)  // 100 MB default
    , maxInstructions_(1000000)           // 1 million instructions default
    , enabled_(true) {

    // Initialize restricted modules.
    // NOTE: "coroutine" is intentionally NOT restricted by default because the
    // engine exposes coroutine-based features; nil-ing it here would break them.
    restrictedModules_ = {
        "os", "io", "debug", "package"
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

const std::unordered_set<std::string>& SandboxConfig::getRestrictedFunctions() const {
    return restrictedFunctions_;
}

const std::unordered_set<std::string>& SandboxConfig::getAllowedModules() const {
    return allowedModules_;
}

const std::unordered_set<std::string>& SandboxConfig::getRestrictedModules() const {
    return restrictedModules_;
}

SandboxManager::SandboxManager()
    : config_(std::make_unique<SandboxConfig>()) {
}

SandboxManager::~SandboxManager() = default;

SandboxConfig& SandboxManager::getConfig() {
    return *config_;
}

void SandboxManager::applySandbox(lua_State* lua) {
    if (!lua) {
        return;  // Silently ignore null Lua state
    }

    if (!config_->isEnabled()) {
        return;  // Sandboxing is disabled
    }

    {
        std::lock_guard<std::mutex> lock(statesMutex_);
        if (sandboxedStates_.find(lua) != sandboxedStates_.end()) {
            return;  // Already sandboxed
        }
    }

    try {
        // Restrict dangerous functions and modules
        restrictDangerousFunctions(lua);
        restrictDangerousModules(lua);

        // Create and register enforcement state, then install the real
        // memory cap and instruction limit against it.
        {
            std::lock_guard<std::mutex> lock(statesMutex_);
            auto data = std::make_unique<SandboxStateData>();
            SandboxStateData* raw = data.get();
            sandboxedStates_[lua] = std::move(data);

            // Make the per-state data reachable from the instruction hook.
            lua_pushlightuserdata(lua, const_cast<char*>(&kSandboxDataKey));
            lua_pushlightuserdata(lua, raw);
            lua_rawset(lua, LUA_REGISTRYINDEX);
        }

        setMemoryLimit(lua, config_->getMaxMemory());
        setInstructionLimit(lua, config_->getMaxInstructions());

        auto log = logger();
        if (log) {
            log->info("Applied sandbox restrictions to Lua state");
        }
    } catch (const std::exception& e) {
        {
            std::lock_guard<std::mutex> lock(statesMutex_);
            sandboxedStates_.erase(lua);
        }
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

    std::lock_guard<std::mutex> lock(statesMutex_);
    auto it = sandboxedStates_.find(lua);
    if (it == sandboxedStates_.end()) {
        return;
    }

    SandboxStateData* d = it->second.get();

    // Disable the instruction hook.
    lua_sethook(lua, nullptr, 0, 0);

    // Clear the registry pointer.
    lua_pushlightuserdata(lua, const_cast<char*>(&kSandboxDataKey));
    lua_pushnil(lua);
    lua_rawset(lua, LUA_REGISTRYINDEX);

    // Restore the original allocator (our limitAlloc and the default both use
    // std::realloc/std::free, so previously tracked blocks remain valid).
    if (d && d->allocInstalled) {
        lua_setallocf(lua, d->origAlloc, d->origUd);
    }

    sandboxedStates_.erase(it);

    auto log = logger();
    if (log) {
        log->info("Removed sandbox restrictions from Lua state");
    }
}

bool SandboxManager::isSandboxed(lua_State* lua) const {
    if (!lua) {
        return false;
    }

    std::lock_guard<std::mutex> lock(statesMutex_);
    return sandboxedStates_.find(lua) != sandboxedStates_.end();
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

    // Defense-in-depth lexical scan. This is intentionally a denylist and is NOT
    // a substitute for the runtime restrictions (global removal + instruction and
    // memory limits) applied by applySandbox(); a determined attacker can still
    // obfuscate names (e.g. _G["o".."s"]). It exists to reject obvious attempts.

    // Dotted module accesses are unambiguous; a substring match is sufficient.
    static const std::vector<std::string> dangerousModulePrefixes = {
        "os.", "io.", "debug.", "package."
    };
    std::string lowerCode = code;
    std::transform(lowerCode.begin(), lowerCode.end(), lowerCode.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    for (const auto& prefix : dangerousModulePrefixes) {
        if (lowerCode.find(prefix) != std::string::npos) {
            throw SandboxViolationException("Dangerous pattern detected: " + prefix);
        }
    }

    // Bare global names are matched on word boundaries so innocent identifiers
    // such as "payload", "reload", or "download" are not flagged.
    static const std::regex dangerousNames(
        R"(\b(load|loadstring|loadfile|dofile|require|module|setmetatable|)"
        R"(getmetatable|rawget|rawset|rawequal|collectgarbage|gcinfo|newproxy|)"
        R"(system|exec|spawn|popen)\b)",
        std::regex::icase);
    std::smatch match;
    if (std::regex_search(code, match, dangerousNames)) {
        throw SandboxViolationException("Dangerous pattern detected: " + match.str(1));
    }
}

void SandboxManager::restrictDangerousFunctions(lua_State* lua) {
    if (!lua) {
        return;
    }

    // Remove dangerous global functions
    for (const auto& function : config_->getRestrictedFunctions()) {
        lua_pushnil(lua);
        lua_setglobal(lua, function.c_str());
    }

    // Also check for functions in allowed modules that might be dangerous
    for (const auto& module [[maybe_unused]] : config_->getAllowedModules()) {
        // This is a simplified approach - in practice, you'd want to check
        // each function in each module against the restricted functions list
    }
}

void SandboxManager::restrictDangerousModules(lua_State* lua) {
    if (!lua) {
        return;
    }

    // Remove dangerous modules from global namespace
    for (const auto& module : config_->getRestrictedModules()) {
        lua_pushnil(lua);
        lua_setglobal(lua, module.c_str());
    }
}

void SandboxManager::setMemoryLimit(lua_State* lua, size_t maxBytes) {
    if (!lua) {
        return;
    }

    std::lock_guard<std::mutex> lock(statesMutex_);
    auto it = sandboxedStates_.find(lua);
    if (it == sandboxedStates_.end()) {
        return;  // Only meaningful on a sandboxed state
    }
    SandboxStateData* d = it->second.get();
    d->memLimit = maxBytes;

    // Install the capping allocator once, preserving the original so it can be
    // restored later. The limit tracks allocations made from this point on
    // (an approximate cap on growth rather than total state size).
    if (!d->allocInstalled) {
        d->origAlloc = lua_getallocf(lua, &d->origUd);
        lua_setallocf(lua, sandboxLimitAlloc, d);
        d->allocInstalled = true;
    }
}

void SandboxManager::setInstructionLimit(lua_State* lua, size_t maxInstructions) {
    if (!lua) {
        return;
    }

    std::lock_guard<std::mutex> lock(statesMutex_);
    auto it = sandboxedStates_.find(lua);
    if (it == sandboxedStates_.end()) {
        return;  // Only meaningful on a sandboxed state
    }
    SandboxStateData* d = it->second.get();
    d->instructionLimit = maxInstructions;
    d->instructionCount = 0;

    if (maxInstructions == 0) {
        lua_sethook(lua, nullptr, 0, 0);  // unlimited
    } else {
        lua_sethook(lua, sandboxInstructionHook, LUA_MASKCOUNT, kInstructionHookStep);
    }
}

SandboxManager& getSandboxManager() {
    // Function-local static: thread-safe initialization (C++11) and no leak.
    static SandboxManager instance;
    return instance;
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