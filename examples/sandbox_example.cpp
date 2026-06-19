/**
 * @file sandbox_example.cpp
 * @brief Example demonstrating Lua sandboxing with FastRules
 * 
 * This example shows how to use the sandbox to restrict Lua execution
 * and prevent malicious code execution.
 */

#include "fastrules/sandbox.hpp"
#include "fastrules/input_validator.hpp"
#include <iostream>
#include <lua.hpp>

using namespace fastrules;

int main() {
    std::cout << "FastRules Sandbox Example\n";
    std::cout << "========================\n\n";
    
    try {
        // Create a Lua state
        lua_State* lua = luaL_newstate();
        if (!lua) {
            std::cerr << "Failed to create Lua state\n";
            return 1;
        }
        
        // Load standard libraries
        luaL_openlibs(lua);
        
        // Get sandbox manager
        auto& manager = getSandboxManager();
        auto& config = manager.getConfig();
        
        // Example 1: Sandbox configuration
        std::cout << "1. Sandbox configuration:\n";
        
        std::cout << "   Sandboxing enabled: " << (config.isEnabled() ? "yes" : "no") << "\n";
        std::cout << "   Max memory: " << config.getMaxMemory() << " bytes\n";
        std::cout << "   Max instructions: " << config.getMaxInstructions() << "\n";
        
        // Show some restricted modules and functions
        std::cout << "   Restricted modules: os, io, debug, package, coroutine\n";
        std::cout << "   Restricted functions: dofile, loadfile, load, loadstring, require\n";
        std::cout << "   Allowed modules: math, string, table, bit32\n";
        std::cout << "   Allowed functions: print, type, next, ipairs, pairs, tonumber, tostring\n\n";
        
        // Example 2: Code validation
        std::cout << "2. Code validation:\n";
        
        std::vector<std::string> testCodes = {
            "return 1 + 1",                           // Safe
            "return math.sqrt(16)",                   // Safe
            "return string.len('hello')",             // Safe
            "return os.execute('ls')",                // Dangerous
            "return io.open('file.txt')",             // Dangerous
            "return dofile('malicious.lua')"          // Dangerous
        };
        
        for (const auto& code : testCodes) {
            try {
                manager.validateCode(code);
                std::cout << "   ✓ Valid: " << code << "\n";
            } catch (const SandboxViolationException& e) {
                std::cout << "   ✗ Invalid: " << code << " (" << e.what() << ")\n";
            }
        }
        std::cout << "\n";
        
        // Example 3: Applying sandbox restrictions
        std::cout << "3. Applying sandbox restrictions:\n";
        
        std::cout << "   Lua state sandboxed: " << (manager.isSandboxed(lua) ? "yes" : "no") << "\n";
        
        // Apply sandbox
        manager.applySandbox(lua);
        std::cout << "   Applied sandbox restrictions\n";
        std::cout << "   Lua state sandboxed: " << (manager.isSandboxed(lua) ? "yes" : "no") << "\n";
        
        // Try to apply sandbox again (should not throw)
        manager.applySandbox(lua);
        std::cout << "   Applied sandbox restrictions again (no effect)\n\n";
        
        // Example 4: Testing restricted functions in Lua
        std::cout << "4. Testing restricted functions in Lua:\n";
        
        std::vector<std::string> luaTests = {
            "return math.abs(-5)",                    // Should work
            "return string.upper('hello')",           // Should work
            "return os.date()",                       // Should fail
            "return io.write('test')"                 // Should fail
        };
        
        for (const auto& test : luaTests) {
            try {
                if (luaL_dostring(lua, test.c_str()) == LUA_OK) {
                    // Get result
                    if (lua_isnumber(lua, -1)) {
                        double result = lua_tonumber(lua, -1);
                        std::cout << "   ✓ Success: " << test << " = " << result << "\n";
                    } else if (lua_isstring(lua, -1)) {
                        const char* result = lua_tostring(lua, -1);
                        std::cout << "   ✓ Success: " << test << " = " << result << "\n";
                    } else {
                        std::cout << "   ✓ Success: " << test << "\n";
                    }
                    lua_pop(lua, 1);  // Remove result from stack
                } else {
                    const char* error = lua_tostring(lua, -1);
                    std::cout << "   ✗ Error: " << test << " (" << error << ")\n";
                    lua_pop(lua, 1);  // Remove error from stack
                }
            } catch (const std::exception& e) {
                std::cout << "   ✗ Exception: " << test << " (" << e.what() << ")\n";
            }
        }
        std::cout << "\n";
        
        // Example 5: SandboxGuard usage
        std::cout << "5. SandboxGuard usage:\n";
        
        // Test that state is not sandboxed
        manager.removeSandbox(lua);
        std::cout << "   Lua state sandboxed before guard: " << (manager.isSandboxed(lua) ? "yes" : "no") << "\n";
        
        {
            SandboxGuard guard(lua);
            std::cout << "   Inside guard scope - Lua state sandboxed: " << (manager.isSandboxed(lua) ? "yes" : "no") << "\n";
            
            // Test a safe operation
            if (luaL_dostring(lua, "return math.pi") == LUA_OK) {
                double pi = lua_tonumber(lua, -1);
                std::cout << "   ✓ Safe operation succeeded: math.pi = " << pi << "\n";
                lua_pop(lua, 1);
            }
        }
        
        std::cout << "   Outside guard scope - Lua state sandboxed: " << (manager.isSandboxed(lua) ? "yes" : "no") << "\n\n";
        
        // Example 6: Custom sandbox configuration
        std::cout << "6. Custom sandbox configuration:\n";
        
        // Add custom restrictions
        config.addRestrictedModule("network");
        config.addRestrictedFunction("http.get");
        config.addAllowedModule("custom_math");
        config.addAllowedFunction("custom_utility");
        
        std::cout << "   Added custom restrictions:\n";
        std::cout << "     Restricted module: network\n";
        std::cout << "     Restricted function: http.get\n";
        std::cout << "     Allowed module: custom_math\n";
        std::cout << "     Allowed function: custom_utility\n";
        
        // Test custom configuration
        config.removeRestrictedModule("network");
        config.removeRestrictedFunction("http.get");
        config.removeAllowedModule("custom_math");
        config.removeAllowedFunction("custom_utility");
        
        std::cout << "   Removed custom restrictions\n\n";
        
        // Example 7: Memory and instruction limits
        std::cout << "7. Memory and instruction limits:\n";
        
        std::cout << "   Current max memory: " << config.getMaxMemory() << " bytes\n";
        std::cout << "   Current max instructions: " << config.getMaxInstructions() << "\n";
        
        // Modify limits
        config.setMaxMemory(1024 * 1024 * 50);    // 50 MB
        config.setMaxInstructions(500000);        // 500k instructions
        
        std::cout << "   Modified max memory: " << config.getMaxMemory() << " bytes\n";
        std::cout << "   Modified max instructions: " << config.getMaxInstructions() << "\n\n";
        
        // Example 8: Disabling sandbox
        std::cout << "8. Disabling sandbox:\n";
        
        std::cout << "   Sandboxing enabled: " << (config.isEnabled() ? "yes" : "no") << "\n";
        
        config.setEnabled(false);
        std::cout << "   Disabled sandboxing\n";
        std::cout << "   Sandboxing enabled: " << (config.isEnabled() ? "yes" : "no") << "\n";
        
        // Test validation with sandboxing disabled
        try {
            manager.validateCode("return os.execute('ls')");  // This would normally fail
            std::cout << "   ✓ Validation passed with sandboxing disabled\n";
        } catch (const SandboxViolationException& e) {
            std::cout << "   ✗ Validation failed even with sandboxing disabled: " << e.what() << "\n";
        }
        
        // Re-enable sandboxing
        config.setEnabled(true);
        std::cout << "   Re-enabled sandboxing\n";
        std::cout << "   Sandboxing enabled: " << (config.isEnabled() ? "yes" : "no") << "\n\n";
        
        // Clean up Lua state
        lua_close(lua);
        
        std::cout << "Sandbox example completed successfully!\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}