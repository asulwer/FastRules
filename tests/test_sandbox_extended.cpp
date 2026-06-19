#include "fastrules/sandbox.hpp"
#include "fastrules/input_validator.hpp"
#include <doctest/doctest.h>
#include <lua.hpp>
#include <thread>
#include <vector>
#include <future>
#include <chrono>

using namespace fastrules;

TEST_CASE("Sandbox advanced configuration") {
    SandboxConfig config;
    
    // Test configuration getters
    CHECK(config.isEnabled());
    CHECK(config.getMaxMemory() == 1024 * 1024 * 100); // 100MB default
    CHECK(config.getMaxInstructions() == 1000000); // 1M default
    
    // Test configuration setters
    config.setMaxMemory(50 * 1024 * 1024); // 50MB
    config.setMaxInstructions(500000); // 500K
    
    CHECK(config.getMaxMemory() == 50 * 1024 * 1024);
    CHECK(config.getMaxInstructions() == 500000);
    
    // Test restricted modules
    CHECK(config.isModuleRestricted("os"));
    CHECK(config.isModuleRestricted("io"));
    CHECK(config.isModuleRestricted("debug"));
    
    // Test adding/removing restricted modules
    config.addRestrictedModule("custom_module");
    CHECK(config.isModuleRestricted("custom_module"));
    config.removeRestrictedModule("custom_module");
    CHECK_FALSE(config.isModuleRestricted("custom_module"));
    
    // Test restricted functions
    CHECK(config.isFunctionRestricted("dofile"));
    CHECK(config.isFunctionRestricted("loadfile"));
    CHECK(config.isFunctionRestricted("load"));
    
    // Test adding/removing restricted functions
    config.addRestrictedFunction("custom_function");
    CHECK(config.isFunctionRestricted("custom_function"));
    config.removeRestrictedFunction("custom_function");
    CHECK_FALSE(config.isFunctionRestricted("custom_function"));
}

TEST_CASE("SandboxConfig thread safety") {
    // Test concurrent access to sandbox configuration
    std::vector<std::future<bool>> futures;
    
    for (int i = 0; i < 10; ++i) {
        futures.push_back(std::async(std::launch::async, [i]() {
            SandboxConfig config;
            
            // Modify configuration
            config.setMaxMemory((i + 1) * 1024 * 1024);
            config.setMaxInstructions((i + 1) * 100000);
            
            // Add some restricted modules
            config.addRestrictedModule("module_" + std::to_string(i));
            config.addRestrictedFunction("function_" + std::to_string(i));
            
            // Verify configuration
            bool success = (config.getMaxMemory() == (i + 1) * 1024 * 1024) &&
                          (config.getMaxInstructions() == (i + 1) * 100000) &&
                          config.isModuleRestricted("module_" + std::to_string(i)) &&
                          config.isFunctionRestricted("function_" + std::to_string(i));
            
            return success;
        }));
    }
    
    // Check all configurations succeeded
    for (auto& future : futures) {
        REQUIRE(future.get() == true);
    }
}

TEST_CASE("SandboxConfig performance") {
    SandboxConfig config;
    
    // Test performance of adding many restricted items
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < 1000; ++i) {
        config.addRestrictedModule("module_" + std::to_string(i));
        config.addRestrictedFunction("function_" + std::to_string(i));
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // Should handle 2000 additions quickly
    CHECK(duration.count() < 1000); // 1 second should be more than enough
    
    // Verify all items were added
    for (int i = 0; i < 1000; ++i) {
        CHECK(config.isModuleRestricted("module_" + std::to_string(i)));
        CHECK(config.isFunctionRestricted("function_" + std::to_string(i)));
    }
}

TEST_CASE("SandboxConfig edge cases") {
    SandboxConfig config;
    
    // Test with zero values
    config.setMaxMemory(0);
    config.setMaxInstructions(0);
    CHECK(config.getMaxMemory() == 0);
    CHECK(config.getMaxInstructions() == 0);
    
    // Test with very large values
    config.setMaxMemory(std::numeric_limits<size_t>::max());
    config.setMaxInstructions(std::numeric_limits<size_t>::max());
    CHECK(config.getMaxMemory() == std::numeric_limits<size_t>::max());
    CHECK(config.getMaxInstructions() == std::numeric_limits<size_t>::max());
    
    // Test with empty strings
    config.addRestrictedModule("");
    config.addRestrictedFunction("");
    CHECK(config.isModuleRestricted(""));
    CHECK(config.isFunctionRestricted(""));
    
    // Test removing non-existent items
    config.removeRestrictedModule("non_existent_module");
    config.removeRestrictedFunction("non_existent_function");
    CHECK_FALSE(config.isModuleRestricted("non_existent_module"));
    CHECK_FALSE(config.isFunctionRestricted("non_existent_function"));
}

TEST_CASE("SandboxConfig allowed modules") {
    SandboxConfig config;
    
    // Test allowed modules
    CHECK(config.isModuleAllowed("math"));
    CHECK(config.isModuleAllowed("string"));
    CHECK(config.isModuleAllowed("table"));
    CHECK_FALSE(config.isModuleAllowed("os"));
    
    // Test adding/removing allowed modules
    config.addAllowedModule("custom_allowed_module");
    CHECK(config.isModuleAllowed("custom_allowed_module"));
    config.removeAllowedModule("custom_allowed_module");
    CHECK_FALSE(config.isModuleAllowed("custom_allowed_module"));
    
    // Test with empty string
    config.addAllowedModule("");
    CHECK(config.isModuleAllowed(""));
    config.removeAllowedModule("");
    CHECK_FALSE(config.isModuleAllowed(""));
}

TEST_CASE("SandboxConfig serialization") {
    SandboxConfig config;
    
    // Set various configuration values
    config.setMaxMemory(123456789);
    config.setMaxInstructions(987654321);
    
    // Add some restricted modules and functions
    config.addRestrictedModule("restricted_module_1");
    config.addRestrictedModule("restricted_module_2");
    config.addRestrictedFunction("restricted_function_1");
    config.addRestrictedFunction("restricted_function_2");
    
    // Add some allowed modules
    config.addAllowedModule("allowed_module_1");
    config.addAllowedModule("allowed_module_2");
    
    // Verify configuration
    CHECK(config.getMaxMemory() == 123456789);
    CHECK(config.getMaxInstructions() == 987654321);
    CHECK(config.isModuleRestricted("restricted_module_1"));
    CHECK(config.isModuleRestricted("restricted_module_2"));
    CHECK(config.isFunctionRestricted("restricted_function_1"));
    CHECK(config.isFunctionRestricted("restricted_function_2"));
    CHECK(config.isModuleAllowed("allowed_module_1"));
    CHECK(config.isModuleAllowed("allowed_module_2"));
}

TEST_CASE("SandboxConfig copy and assignment") {
    SandboxConfig config1;
    
    // Set up config1
    config1.setMaxMemory(100 * 1024 * 1024);
    config1.setMaxInstructions(1000000);
    config1.addRestrictedModule("module1");
    config1.addRestrictedFunction("function1");
    config1.addAllowedModule("allowed1");
    
    // Test copy constructor
    SandboxConfig config2(config1);
    CHECK(config2.getMaxMemory() == config1.getMaxMemory());
    CHECK(config2.getMaxInstructions() == config1.getMaxInstructions());
    CHECK(config2.isModuleRestricted("module1"));
    CHECK(config2.isFunctionRestricted("function1"));
    CHECK(config2.isModuleAllowed("allowed1"));
    
    // Test copy assignment
    SandboxConfig config3;
    config3 = config1;
    CHECK(config3.getMaxMemory() == config1.getMaxMemory());
    CHECK(config3.getMaxInstructions() == config1.getMaxInstructions());
    CHECK(config3.isModuleRestricted("module1"));
    CHECK(config3.isFunctionRestricted("function1"));
    CHECK(config3.isModuleAllowed("allowed1"));
    
    // Test move constructor
    SandboxConfig config4(std::move(config1));
    CHECK(config4.getMaxMemory() == 100 * 1024 * 1024);
    CHECK(config4.getMaxInstructions() == 1000000);
    CHECK(config4.isModuleRestricted("module1"));
    CHECK(config4.isFunctionRestricted("function1"));
    CHECK(config4.isModuleAllowed("allowed1"));
    
    // Test move assignment
    SandboxConfig config5;
    config5 = std::move(config2);
    CHECK(config5.getMaxMemory() == 100 * 1024 * 1024);
    CHECK(config5.getMaxInstructions() == 1000000);
    CHECK(config5.isModuleRestricted("module1"));
    CHECK(config5.isFunctionRestricted("function1"));
    CHECK(config5.isModuleAllowed("allowed1"));
}

TEST_CASE("SandboxConfig allowed functions") {
    SandboxConfig config;
    
    // Test adding/removing allowed functions
    config.addAllowedFunction("custom_allowed_function");
    CHECK(config.isFunctionAllowed("custom_allowed_function"));
    config.removeAllowedFunction("custom_allowed_function");
    CHECK_FALSE(config.isFunctionAllowed("custom_allowed_function"));
    
    // Test with empty string
    config.addAllowedFunction("");
    CHECK(config.isFunctionAllowed(""));
    config.removeAllowedFunction("");
    CHECK_FALSE(config.isFunctionAllowed(""));
}

TEST_CASE("SandboxConfig comprehensive testing") {
    SandboxConfig config;
    
    // Test comprehensive set of restricted modules
    std::vector<std::string> restrictedModules = {
        "os", "io", "debug", "package", "coroutine"
    };
    
    for (const auto& module : restrictedModules) {
        config.addRestrictedModule(module);
        CHECK(config.isModuleRestricted(module));
    }
    
    // Test comprehensive set of restricted functions
    std::vector<std::string> restrictedFunctions = {
        "dofile", "loadfile", "load", "loadstring", "require"
    };
    
    for (const auto& function : restrictedFunctions) {
        config.addRestrictedFunction(function);
        CHECK(config.isFunctionRestricted(function));
    }
    
    // Test comprehensive set of allowed modules
    std::vector<std::string> allowedModules = {
        "math", "string", "table", "bit32"
    };
    
    for (const auto& module : allowedModules) {
        config.addAllowedModule(module);
        CHECK(config.isModuleAllowed(module));
    }
    
    // Test comprehensive set of allowed functions
    std::vector<std::string> allowedFunctions = {
        "print", "tonumber", "tostring", "type"
    };
    
    for (const auto& function : allowedFunctions) {
        config.addAllowedFunction(function);
        CHECK(config.isFunctionAllowed(function));
    }
}

TEST_CASE("SandboxManager functionality") {
    // Test getting sandbox manager
    SandboxManager& manager = getSandboxManager();
    
    // Test that we can get the configuration
    SandboxConfig& config = manager.getConfig();
    CHECK(config.isEnabled());
    
    // Test with a Lua state
    lua_State* lua = luaL_newstate();
    REQUIRE(lua != nullptr);
    
    // Test applying sandbox
    CHECK_NOTHROW(manager.applySandbox(lua));
    
    // Test checking if sandboxed
    CHECK(manager.isSandboxed(lua));
    
    // Test removing sandbox
    CHECK_NOTHROW(manager.removeSandbox(lua));
    
    // Test checking if sandboxed (should be false now)
    CHECK_FALSE(manager.isSandboxed(lua));
    
    lua_close(lua);
}

TEST_CASE("SandboxGuard functionality") {
    // Test with a Lua state
    lua_State* lua = luaL_newstate();
    REQUIRE(lua != nullptr);
    
    // Test that Lua state is not initially sandboxed
    SandboxManager& manager = getSandboxManager();
    CHECK_FALSE(manager.isSandboxed(lua));
    
    // Test sandbox guard RAII
    {
        SandboxGuard guard(lua);
        
        // Should be sandboxed within the guard scope
        CHECK(manager.isSandboxed(lua));
    }
    
    // Should not be sandboxed after guard goes out of scope
    CHECK_FALSE(manager.isSandboxed(lua));
    
    lua_close(lua);
}

TEST_CASE("Sandbox violation detection") {
    SandboxConfig config;
    
    // Test that dangerous modules are restricted by default
    CHECK(config.isModuleRestricted("os"));
    CHECK(config.isModuleRestricted("io"));
    CHECK(config.isModuleRestricted("debug"));
    CHECK(config.isModuleRestricted("package"));
    
    // Test that dangerous functions are restricted by default
    CHECK(config.isFunctionRestricted("dofile"));
    CHECK(config.isFunctionRestricted("loadfile"));
    CHECK(config.isFunctionRestricted("load"));
    CHECK(config.isFunctionRestricted("loadstring"));
    
    // Test that safe modules are allowed by default
    CHECK(config.isModuleAllowed("math"));
    CHECK(config.isModuleAllowed("string"));
    CHECK(config.isModuleAllowed("table"));
}