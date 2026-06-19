#include "fastrules/sandbox.hpp"
#include "fastrules/input_validator.hpp"
#include <doctest/doctest.h>
#include <lua.hpp>

TEST_CASE("SandboxConfig basic functionality") {
    fastrules::SandboxConfig config;
    
    // Test default values
    CHECK(config.isEnabled());
    CHECK(config.getMaxMemory() == 1024 * 1024 * 100);
    CHECK(config.getMaxInstructions() == 1000000);
    
    // Test restricted modules
    CHECK(config.isModuleRestricted("os"));
    CHECK(config.isModuleRestricted("io"));
    CHECK(config.isModuleRestricted("debug"));
    CHECK_FALSE(config.isModuleRestricted("math"));
    
    // Test adding/removing restricted modules
    config.addRestrictedModule("custom_module");
    CHECK(config.isModuleRestricted("custom_module"));
    config.removeRestrictedModule("custom_module");
    CHECK_FALSE(config.isModuleRestricted("custom_module"));
    
    // Test restricted functions
    CHECK(config.isFunctionRestricted("dofile"));
    CHECK(config.isFunctionRestricted("loadfile"));
    CHECK(config.isFunctionRestricted("load"));
    CHECK_FALSE(config.isFunctionRestricted("print"));
    
    // Test adding/removing restricted functions
    config.addRestrictedFunction("custom_function");
    CHECK(config.isFunctionRestricted("custom_function"));
    config.removeRestrictedFunction("custom_function");
    CHECK_FALSE(config.isFunctionRestricted("custom_function"));
    
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
    
    // Test allowed functions
    CHECK(config.isFunctionAllowed("print"));
    CHECK(config.isFunctionAllowed("type"));
    CHECK_FALSE(config.isFunctionAllowed("os.execute"));
    
    // Test adding/removing allowed functions
    config.addAllowedFunction("custom_allowed_function");
    CHECK(config.isFunctionAllowed("custom_allowed_function"));
    config.removeAllowedFunction("custom_allowed_function");
    CHECK_FALSE(config.isFunctionAllowed("custom_allowed_function"));
    
    // Test enabling/disabling
    config.setEnabled(false);
    CHECK_FALSE(config.isEnabled());
    config.setEnabled(true);
    CHECK(config.isEnabled());
    
    // Test memory and instruction limits
    config.setMaxMemory(1024 * 1024 * 50);  // 50 MB
    config.setMaxInstructions(500000);      // 500k instructions
    CHECK(config.getMaxMemory() == 1024 * 1024 * 50);
    CHECK(config.getMaxInstructions() == 500000);
}

TEST_CASE("SandboxManager basic functionality") {
    auto& manager = fastrules::getSandboxManager();
    auto& config = manager.getConfig();
    
    // Test that we can get the config
    CHECK(config.isEnabled());
    
    // Test validation with clean code
    CHECK_NOTHROW(manager.validateCode("return 1 + 1"));
    CHECK_NOTHROW(manager.validateCode("return math.sqrt(16)"));
    
    // Test validation with dangerous code
    CHECK_THROWS_AS(manager.validateCode("return os.execute('ls')"), fastrules::SandboxViolationException);
    CHECK_THROWS_AS(manager.validateCode("return io.open('file.txt')"), fastrules::SandboxViolationException);
    
    // Test with sandboxing disabled
    config.setEnabled(false);
    CHECK_NOTHROW(manager.validateCode("return os.execute('ls')"));  // Should not throw when disabled
    config.setEnabled(true);
}

TEST_CASE("SandboxGuard basic functionality") {
    // Create a Lua state
    lua_State* lua = luaL_newstate();
    REQUIRE(lua != nullptr);
    
    // Load standard libraries
    luaL_openlibs(lua);
    
    // Test that state is not initially sandboxed
    auto& manager = fastrules::getSandboxManager();
    CHECK_FALSE(manager.isSandboxed(lua));
    
    // Test sandbox guard
    {
        fastrules::SandboxGuard guard(lua);
        CHECK(manager.isSandboxed(lua));
    }
    
    // Test that state is no longer sandboxed after guard destruction
    CHECK_FALSE(manager.isSandboxed(lua));
    
    // Clean up
    lua_close(lua);
}

TEST_CASE("SandboxManager with Lua state") {
    // Create a Lua state
    lua_State* lua = luaL_newstate();
    REQUIRE(lua != nullptr);
    
    // Load standard libraries
    luaL_openlibs(lua);
    
    auto& manager = fastrules::getSandboxManager();
    
    // Test applying sandbox
    CHECK_FALSE(manager.isSandboxed(lua));
    CHECK_NOTHROW(manager.applySandbox(lua));
    CHECK(manager.isSandboxed(lua));
    
    // Test applying sandbox again (should not throw)
    CHECK_NOTHROW(manager.applySandbox(lua));
    CHECK(manager.isSandboxed(lua));
    
    // Test removing sandbox
    manager.removeSandbox(lua);
    CHECK_FALSE(manager.isSandboxed(lua));
    
    // Test removing sandbox again (should not throw)
    CHECK_NOTHROW(manager.removeSandbox(lua));
    CHECK_FALSE(manager.isSandboxed(lua));
    
    // Test with null Lua state
    CHECK_NOTHROW(manager.applySandbox(nullptr));
    CHECK_NOTHROW(manager.removeSandbox(nullptr));
    CHECK_FALSE(manager.isSandboxed(nullptr));
    
    // Clean up
    lua_close(lua);
}

TEST_CASE("Sandbox violation detection") {
    auto& manager = fastrules::getSandboxManager();
    auto& config = manager.getConfig();
    
    // Test with sandboxing enabled
    config.setEnabled(true);
    
    // Test dangerous function detection
    CHECK_THROWS_AS(manager.validateCode("os.execute('rm -rf /')"), fastrules::SandboxViolationException);
    CHECK_THROWS_AS(manager.validateCode("io.open('/etc/passwd')"), fastrules::SandboxViolationException);
    CHECK_THROWS_AS(manager.validateCode("dofile('malicious.lua')"), fastrules::SandboxViolationException);
    CHECK_THROWS_AS(manager.validateCode("load('malicious_code')"), fastrules::SandboxViolationException);
    
    // Test clean code
    CHECK_NOTHROW(manager.validateCode("return math.abs(-5)"));
    CHECK_NOTHROW(manager.validateCode("return string.len('hello')"));
    CHECK_NOTHROW(manager.validateCode("return table.concat({'a', 'b', 'c'})"));
}

TEST_CASE("Sandbox configuration modification") {
    fastrules::SandboxConfig config;
    
    // Test modifying configuration
    config.setMaxMemory(1024 * 1024 * 25);   // 25 MB
    config.setMaxInstructions(250000);        // 250k instructions
    
    CHECK(config.getMaxMemory() == 1024 * 1024 * 25);
    CHECK(config.getMaxInstructions() == 250000);
    
    // Test adding custom restrictions
    config.addRestrictedModule("network");
    config.addRestrictedFunction("http.get");
    
    CHECK(config.isModuleRestricted("network"));
    CHECK(config.isFunctionRestricted("http.get"));
    
    // Test removing custom restrictions
    config.removeRestrictedModule("network");
    config.removeRestrictedFunction("http.get");
    
    CHECK_FALSE(config.isModuleRestricted("network"));
    CHECK_FALSE(config.isFunctionRestricted("http.get"));
    
    // Test adding custom allowances
    config.addAllowedModule("custom_math");
    config.addAllowedFunction("custom_utility");
    
    CHECK(config.isModuleAllowed("custom_math"));
    CHECK(config.isFunctionAllowed("custom_utility"));
    
    // Test removing custom allowances
    config.removeAllowedModule("custom_math");
    config.removeAllowedFunction("custom_utility");
    
    CHECK_FALSE(config.isModuleAllowed("custom_math"));
    CHECK_FALSE(config.isFunctionAllowed("custom_utility"));
}