/**
 * @file sandbox.hpp
 * @brief Lua sandboxing for FastRules
 * 
 * Provides sandboxing mechanisms to restrict Lua execution
 * and prevent malicious code execution.
 */

#pragma once

#include <string>
#include <unordered_set>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <stdexcept>

// Forward declarations
struct lua_State;

namespace fastrules {

// Per-state sandbox enforcement data (instruction/memory counters and the
// saved original allocator). Defined in sandbox.cpp.
struct SandboxStateData;

/**
 * @brief Exception thrown when sandbox restrictions are violated
 */
class SandboxViolationException : public std::runtime_error {
public:
    explicit SandboxViolationException(const std::string& message) 
        : std::runtime_error("Sandbox violation: " + message) {}
};

/**
 * @brief Lua sandbox configuration
 * 
 * Defines what is allowed and restricted in the Lua environment.
 */
class SandboxConfig {
private:
    // Restricted modules
    std::unordered_set<std::string> restrictedModules_;
    
    // Restricted functions
    std::unordered_set<std::string> restrictedFunctions_;
    
    // Allowed modules
    std::unordered_set<std::string> allowedModules_;
    
    // Allowed functions
    std::unordered_set<std::string> allowedFunctions_;
    
    // Memory limits
    size_t maxMemoryBytes_;
    
    // Maximum number of instructions
    size_t maxInstructions_;
    
    // Enable/disable sandboxing
    bool enabled_;

public:
    /**
     * @brief Construct sandbox configuration
     */
    SandboxConfig();

    /**
     * @brief Enable/disable sandboxing
     * 
     * @param enabled true to enable sandboxing
     */
    void setEnabled(bool enabled);

    /**
     * @brief Check if sandboxing is enabled
     * 
     * @return true if sandboxing is enabled
     */
    bool isEnabled() const;

    /**
     * @brief Add restricted module
     * 
     * @param module Module name to restrict
     */
    void addRestrictedModule(const std::string& module);

    /**
     * @brief Remove restricted module
     * 
     * @param module Module name to allow
     */
    void removeRestrictedModule(const std::string& module);

    /**
     * @brief Check if module is restricted
     * 
     * @param module Module name to check
     * @return true if module is restricted
     */
    bool isModuleRestricted(const std::string& module) const;

    /**
     * @brief Add restricted function
     * 
     * @param function Function name to restrict
     */
    void addRestrictedFunction(const std::string& function);

    /**
     * @brief Remove restricted function
     * 
     * @param function Function name to allow
     */
    void removeRestrictedFunction(const std::string& function);

    /**
     * @brief Check if function is restricted
     * 
     * @param function Function name to check
     * @return true if function is restricted
     */
    bool isFunctionRestricted(const std::string& function) const;

    /**
     * @brief Add allowed module
     * 
     * @param module Module name to allow
     */
    void addAllowedModule(const std::string& module);

    /**
     * @brief Remove allowed module
     * 
     * @param module Module name to restrict
     */
    void removeAllowedModule(const std::string& module);

    /**
     * @brief Check if module is allowed
     * 
     * @param module Module name to check
     * @return true if module is allowed
     */
    bool isModuleAllowed(const std::string& module) const;

    /**
     * @brief Add allowed function
     * 
     * @param function Function name to allow
     */
    void addAllowedFunction(const std::string& function);

    /**
     * @brief Remove allowed function
     * 
     * @param function Function name to restrict
     */
    void removeAllowedFunction(const std::string& function);

    /**
     * @brief Check if function is allowed
     * 
     * @param function Function name to check
     * @return true if function is allowed
     */
    bool isFunctionAllowed(const std::string& function) const;

    /**
     * @brief Set maximum memory usage
     * 
     * @param bytes Maximum memory in bytes
     */
    void setMaxMemory(size_t bytes);

    /**
     * @brief Get maximum memory usage
     * 
     * @return Maximum memory in bytes
     */
    size_t getMaxMemory() const;

    /**
     * @brief Set maximum instruction count
     * 
     * @param instructions Maximum instructions
     */
    void setMaxInstructions(size_t instructions);

    /**
     * @brief Get maximum instruction count
     * 
     * @return Maximum instructions
     */
    size_t getMaxInstructions() const;

    /**
     * @brief Get restricted functions
     * 
     * @return Set of restricted function names
     */
    const std::unordered_set<std::string>& getRestrictedFunctions() const;

    /**
     * @brief Get allowed modules
     * 
     * @return Set of allowed module names
     */
    const std::unordered_set<std::string>& getAllowedModules() const;

    /**
     * @brief Get restricted modules
     * 
     * @return Set of restricted module names
     */
    const std::unordered_set<std::string>& getRestrictedModules() const;
};

/**
 * @brief Lua sandbox manager
 * 
 * Manages the Lua sandbox environment and enforces restrictions.
 */
class SandboxManager {
private:
    std::unique_ptr<SandboxConfig> config_;
    std::unordered_map<lua_State*, std::unique_ptr<SandboxStateData>> sandboxedStates_;
    mutable std::mutex statesMutex_;

public:
    /**
     * @brief Construct sandbox manager
     */
    SandboxManager();

    /**
     * @brief Destructor (defined out-of-line where SandboxStateData is complete)
     */
    ~SandboxManager();

    /**
     * @brief Get sandbox configuration
     * 
     * @return Sandbox configuration
     */
    SandboxConfig& getConfig();

    /**
     * @brief Apply sandbox restrictions to Lua state
     * 
     * @param lua Lua state to sandbox
     * @throws SandboxViolationException if restrictions cannot be applied
     */
    void applySandbox(lua_State* lua);

    /**
     * @brief Remove sandbox restrictions from Lua state
     * 
     * @param lua Lua state to unsandbox
     */
    void removeSandbox(lua_State* lua);

    /**
     * @brief Check if Lua state is sandboxed
     * 
     * @param lua Lua state to check
     * @return true if state is sandboxed
     */
    bool isSandboxed(lua_State* lua) const;

    /**
     * @brief Validate Lua code against sandbox restrictions
     * 
     * @param code Lua code to validate
     * @throws SandboxViolationException if code violates restrictions
     */
    void validateCode(const std::string& code) const;

    /**
     * @brief Restrict dangerous Lua functions
     * 
     * @param lua Lua state
     */
    void restrictDangerousFunctions(lua_State* lua);

    /**
     * @brief Restrict dangerous Lua modules
     * 
     * @param lua Lua state
     */
    void restrictDangerousModules(lua_State* lua);

    /**
     * @brief Set memory limit for Lua state
     * 
     * @param lua Lua state
     * @param maxBytes Maximum memory in bytes
     */
    void setMemoryLimit(lua_State* lua, size_t maxBytes);

    /**
     * @brief Set instruction limit for Lua state
     * 
     * @param lua Lua state
     * @param maxInstructions Maximum instructions
     */
    void setInstructionLimit(lua_State* lua, size_t maxInstructions);
};

/**
 * @brief Get global sandbox manager
 * 
 * @return Sandbox manager
 */
SandboxManager& getSandboxManager();

/**
 * @brief RAII-style sandbox guard
 * 
 * Automatically applies and removes sandbox restrictions.
 */
class SandboxGuard {
private:
    lua_State* lua_;
    bool wasSandboxed_;

public:
    /**
     * @brief Construct sandbox guard
     * 
     * @param lua Lua state to sandbox
     */
    explicit SandboxGuard(lua_State* lua);

    /**
     * @brief Destructor - removes sandbox restrictions
     */
    ~SandboxGuard();
};

} // namespace fastrules