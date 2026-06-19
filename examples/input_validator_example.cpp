/**
 * @file input_validator_example.cpp
 * @brief Example demonstrating input validation with FastRules
 * 
 * This example shows how to use input validators to prevent injection
 * attacks and ensure security.
 */

#include "fastrules/input_validator.hpp"
#include <iostream>
#include <vector>

using namespace fastrules;

int main() {
    std::cout << "FastRules Input Validator Example\n";
    std::cout << "=================================\n\n";
    
    try {
        // Get security configuration
        auto& securityConfig = getSecurityConfig();
        
        // Example 1: Lua expression validation
        std::cout << "1. Lua expression validation:\n";
        
        LuaExpressionValidator& exprValidator = securityConfig.getExpressionValidator();
        
        // Test valid expressions
        std::vector<std::string> validExpressions = {
            "return value > 10",
            "return math.sqrt(x) + math.sin(y)",
            "return string.len(text) > 0",
            "return table.concat(items, ', ')"
        };
        
        for (const auto& expr : validExpressions) {
            try {
                exprValidator.validate(expr);
                std::cout << "   ✓ Valid: " << expr << "\n";
            } catch (const ValidationException& e) {
                std::cout << "   ✗ Invalid: " << expr << " (" << e.what() << ")\n";
            }
        }
        
        // Test invalid expressions
        std::vector<std::string> invalidExpressions = {
            "return os.execute('rm -rf /')",  // Dangerous function
            "return io.open('secret.txt')",   // Dangerous function
            "return load('malicious_code')",  // Dangerous function
            std::string(10001, 'a')           // Too long
        };
        
        for (const auto& expr : invalidExpressions) {
            try {
                exprValidator.validate(expr);
                std::cout << "   ✓ Valid: " << expr.substr(0, 30) << "...\n";
            } catch (const ValidationException& e) {
                std::cout << "   ✗ Invalid: " << expr.substr(0, 30) << "... (" << e.what() << ")\n";
            }
        }
        std::cout << "\n";
        
        // Example 2: Parameter validation
        std::cout << "2. Parameter validation:\n";
        
        ParameterValidator& paramValidator = securityConfig.getParameterValidator();
        
        // Test valid parameters
        std::vector<std::pair<std::string, std::string>> validParams = {
            {"value", "42"},
            {"name", "test_parameter"},
            {"description", "This is a test parameter"}
        };
        
        for (const auto& [name, value] : validParams) {
            try {
                paramValidator.validate(name, value);
                std::cout << "   ✓ Valid: " << name << " = " << value << "\n";
            } catch (const ValidationException& e) {
                std::cout << "   ✗ Invalid: " << name << " = " << value << " (" << e.what() << ")\n";
            }
        }
        
        // Test invalid parameters
        std::vector<std::pair<std::string, std::string>> invalidParams = {
            {"", "empty_name"},                    // Empty name
            {"123invalid", "starts_with_number"},  // Invalid name
            {"function", "reserved_name"},         // Reserved name
            {"valid_name", std::string(10001, 'a')} // Too long value
        };
        
        for (const auto& [name, value] : invalidParams) {
            try {
                paramValidator.validate(name, value);
                std::cout << "   ✓ Valid: " << name << " = " << value.substr(0, 30) << "...\n";
            } catch (const ValidationException& e) {
                std::cout << "   ✗ Invalid: " << name << " = " << value.substr(0, 30) << "... (" << e.what() << ")\n";
            }
        }
        std::cout << "\n";
        
        // Example 3: Sanitization
        std::cout << "3. Sanitization:\n";
        
        // Sanitize expressions
        std::vector<std::string> expressionsToSanitize = {
            "return value + 1",           // Already clean
            "return os.execute('hack')",  // Contains dangerous pattern
            std::string(15000, 'a')       // Too long
        };
        
        for (const auto& expr : expressionsToSanitize) {
            std::string sanitized = exprValidator.sanitize(expr);
            std::cout << "   Original length: " << expr.length() 
                      << ", Sanitized length: " << sanitized.length() << "\n";
        }
        
        // Sanitize parameter names
        std::vector<std::string> namesToSanitize = {
            "valid_name",      // Already valid
            "123invalid",      // Starts with number
            "invalid-name",    // Contains hyphen
            "function"         // Reserved word
        };
        
        for (const auto& name : namesToSanitize) {
            std::string sanitized = paramValidator.sanitizeName(name);
            std::cout << "   Original: " << name << ", Sanitized: " << sanitized << "\n";
        }
        
        // Sanitize parameter values
        std::vector<std::string> valuesToSanitize = {
            "valid value",     // Already valid
            "test\0value",     // Contains null byte
            std::string(15000, 'a')  // Too long
        };
        
        for (const auto& value : valuesToSanitize) {
            std::string sanitized = paramValidator.sanitizeValue(value);
            std::cout << "   Original length: " << value.length() 
                      << ", Sanitized length: " << sanitized.length() << "\n";
        }
        std::cout << "\n";
        
        // Example 4: Security configuration
        std::cout << "4. Security configuration:\n";
        
        // Check current settings
        std::cout << "   Sandboxing enabled: " << (securityConfig.isSandboxingEnabled() ? "yes" : "no") << "\n";
        std::cout << "   Max execution time: " << securityConfig.getMaxExecutionTime() << " seconds\n";
        std::cout << "   Max memory usage: " << securityConfig.getMaxMemory() << " bytes\n";
        
        // Modify settings
        securityConfig.setSandboxing(false);
        securityConfig.setMaxExecutionTime(60);
        securityConfig.setMaxMemory(1024 * 1024 * 500);  // 500 MB
        
        std::cout << "   After modification:\n";
        std::cout << "   Sandboxing enabled: " << (securityConfig.isSandboxingEnabled() ? "yes" : "no") << "\n";
        std::cout << "   Max execution time: " << securityConfig.getMaxExecutionTime() << " seconds\n";
        std::cout << "   Max memory usage: " << securityConfig.getMaxMemory() << " bytes\n\n";
        
        // Example 5: Custom function whitelisting
        std::cout << "5. Custom function whitelisting:\n";
        
        // Add custom allowed function
        exprValidator.addAllowedFunction("custom.utility");
        std::cout << "   Added custom function: custom.utility\n";
        
        // Test with custom function
        try {
            exprValidator.validate("return custom.utility(value)");
            std::cout << "   ✓ Valid with custom function\n";
        } catch (const ValidationException& e) {
            std::cout << "   ✗ Invalid: " << e.what() << "\n";
        }
        
        // Remove custom function
        exprValidator.removeAllowedFunction("custom.utility");
        std::cout << "   Removed custom function: custom.utility\n";
        
        // Test again (should fail now)
        try {
            exprValidator.validate("return custom.utility(value)");
            std::cout << "   ✓ Valid with custom function\n";
        } catch (const ValidationException& e) {
            std::cout << "   ✗ Invalid (as expected): " << e.what() << "\n";
        }
        
        std::cout << "\nInput validator example completed successfully!\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}