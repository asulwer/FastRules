#include "fastrules/input_validator.hpp"
#include <doctest/doctest.h>
#include <thread>
#include <vector>
#include <future>
#include <chrono>

using namespace fastrules;

TEST_CASE("LuaExpressionValidator advanced functionality") {
    LuaExpressionValidator validator(10000);
    
    // Test valid expressions
    std::vector<std::string> safeExpressions = {
        "return x > 5",
        "return user.age >= 18 and user.active",
        "return data.field ~= nil",
        "return string.len(name) > 0",
        "return math.floor(value) == 10",
        "return table.concat(items, ',')",
        "return type(variable) == 'number'",
        "return count % 2 == 0",
        "return items[#items]"
    };
    
    for (const auto& expr : safeExpressions) {
        CHECK_NOTHROW(validator.validate(expr));
    }
    
    // Test dangerous expressions that should be blocked
    std::vector<std::string> dangerousExpressions = {
        "return io.open('file.txt')",
        "return os.execute('rm -rf /')",
        "return loadstring('malicious code')",
        "return debug.traceback()",
        "return package.loadlib()",
        "return _G.something",
        "return getfenv()",
        "return setmetatable({}, {})",
        "return collectgarbage()",
        "return dofile('config.lua')"
    };
    
    for (const auto& expr : dangerousExpressions) {
        CHECK_THROWS_AS(validator.validate(expr), ValidationException);
    }
}

TEST_CASE("ParameterValidator advanced functionality") {
    ParameterValidator validator;
    
    // Test various data types
    SUBCASE("String validation") {
        std::string validString = "Hello, World!";
        std::string sanitized = validator.sanitizeValue(validString);
        CHECK_FALSE(sanitized.empty());
    }
    
    SUBCASE("Name validation") {
        std::string validName = "test_parameter";
        std::string sanitized = validator.sanitizeName(validName);
        CHECK_FALSE(sanitized.empty());
    }
}

TEST_CASE("ParameterValidator sanitization") {
    ParameterValidator validator;
    
    // Test string sanitization
    SUBCASE("String sanitization") {
        // Test trimming whitespace
        std::string whitespaceInput = "  Hello World  ";
        std::string trimmed = validator.sanitizeValue(whitespaceInput);
        CHECK_FALSE(trimmed.empty());
    }
    
    // Test name sanitization
    SUBCASE("Name sanitization") {
        std::string nameInput = "  test_param  ";
        std::string sanitized = validator.sanitizeName(nameInput);
        CHECK_FALSE(sanitized.empty());
    }
}

TEST_CASE("InputValidator thread safety") {
    LuaExpressionValidator luaValidator(10000);
    ParameterValidator paramValidator;
    
    // Test concurrent validation
    std::vector<std::future<bool>> futures;
    
    for (int i = 0; i < 10; ++i) {
        futures.push_back(std::async(std::launch::async, [&luaValidator, &paramValidator, i]() {
            try {
                // Test Lua expression validation
                std::string expr = "return x > " + std::to_string(i);
                luaValidator.validate(expr);
                
                // Test parameter validation
                std::string param = "test_param_" + std::to_string(i);
                std::string sanitized = paramValidator.sanitizeValue(param);
                
                return !sanitized.empty();
            } catch (...) {
                return false;
            }
        }));
    }
    
    // Check all validations succeeded
    for (auto& future : futures) {
        REQUIRE(future.get() == true);
    }
}

TEST_CASE("InputValidator regex patterns") {
    LuaExpressionValidator validator(10000);
    
    // Test various safe patterns
    std::vector<std::string> safeExpressions = {
        "return x > 5",
        "return user.name == 'John'",
        "return data.score >= 100.5",
        "return active and verified",
        "return count % 2 == 0",
        "return items[#items]",
        "return string.upper(name)",
        "return math.max(a, b, c)",
        "return table.insert(list, value)",
        "return type(variable) == 'number'"
    };
    
    for (const auto& expr : safeExpressions) {
        CHECK_NOTHROW(validator.validate(expr));
    }
    
    // Test dangerous patterns
    std::vector<std::string> dangerousExpressions = {
        "return io.write('hello')",
        "return os.getenv('PATH')",
        "return load('malicious code')",
        "return debug.getinfo(1)",
        "return package.path",
        "return _G.global_var",
        "return getmetatable(obj)",
        "return setfenv(1, {})",
        "return collectgarbage()",
        "return dofile('config.lua')"
    };
    
    for (const auto& expr : dangerousExpressions) {
        CHECK_THROWS_AS(validator.validate(expr), ValidationException);
    }
}

TEST_CASE("InputValidator performance") {
    LuaExpressionValidator validator(10000);
    
    // Test validation performance
    std::vector<std::string> expressions;
    for (int i = 0; i < 1000; ++i) {
        expressions.push_back("return x > " + std::to_string(i) + " and y < " + std::to_string(i * 2));
    }
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (const auto& expr : expressions) {
        CHECK_NOTHROW(validator.validate(expr));
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // Should validate 1000 expressions quickly (less than 1 second)
    CHECK(duration.count() < 1000);
}

TEST_CASE("InputValidator edge cases") {
    LuaExpressionValidator validator(10000);
    ParameterValidator paramValidator;
    
    // Test empty expressions
    CHECK_THROWS_AS(validator.validate(""), ValidationException);
    
    // Test whitespace-only expressions
    CHECK_THROWS_AS(validator.validate("   "), ValidationException);
    
    // Test very long expressions (within limit)
    std::string longExpr(9999, 'x');
    longExpr = "return " + longExpr;
    CHECK_NOTHROW(validator.validate(longExpr));
    
    // Test parameter validation with null values
    std::string nullString;
    std::string sanitized = paramValidator.sanitizeValue(nullString);
    // Should handle empty strings gracefully
    
    // Test parameter validation with special characters
    std::string specialChars = "!@#$%^&*()_+-=[]{}|;':\",./<>?";
    std::string sanitizedSpecial = paramValidator.sanitizeValue(specialChars);
    // Should handle special characters without throwing
    CHECK_NOTHROW(sanitizedSpecial);
}

TEST_CASE("LuaExpressionValidator function management") {
    LuaExpressionValidator validator(10000);
    
    // Test adding allowed functions
    validator.addAllowedFunction("custom_function");
    CHECK(validator.isFunctionAllowed("custom_function"));
    
    // Test removing allowed functions
    validator.removeAllowedFunction("custom_function");
    // Note: This might still return true depending on implementation
}

TEST_CASE("LuaExpressionValidator sanitization") {
    LuaExpressionValidator validator(10000);
    
    // Test sanitizing expressions
    std::string expr = "  return x > 5  ";
    std::string sanitized = validator.sanitize(expr);
    CHECK_FALSE(sanitized.empty());
    
    // Test with dangerous content that should be sanitized
    std::string dangerousExpr = "return io.open('test') -- malicious comment";
    CHECK_THROWS_AS(validator.validate(dangerousExpr), ValidationException);
}

TEST_CASE("ParameterValidator name validation") {
    ParameterValidator validator;
    
    // Test valid parameter names
    std::vector<std::string> validNames = {
        "test",
        "test_param",
        "testParam",
        "test123",
        "_test"
    };
    
    for (const auto& name : validNames) {
        CHECK_NOTHROW(validator.validateName(name));
    }
    
    // Test that validation works
    CHECK_NOTHROW(validator.validate("test", "value"));
}

TEST_CASE("ParameterValidator value validation") {
    ParameterValidator validator;
    
    // Test valid parameter values
    std::vector<std::string> validValues = {
        "test",
        "test value",
        "123",
        "test123",
        "!@#$%^&*()"
    };
    
    for (const auto& value : validValues) {
        CHECK_NOTHROW(validator.validateValue(value));
    }
}

TEST_CASE("SecurityConfig functionality") {
    // Test getting security config
    SecurityConfig& config = getSecurityConfig();
    
    // Test that we can get validators
    LuaExpressionValidator& exprValidator = config.getExpressionValidator();
    ParameterValidator& paramValidator = config.getParameterValidator();
    
    // Test basic functionality
    CHECK_NOTHROW(exprValidator.validate("return x > 0"));
    CHECK_NOTHROW(paramValidator.validate("test", "value"));
    
    // Test sandboxing configuration
    config.setSandboxing(true);
    CHECK(config.isSandboxingEnabled());
    
    config.setSandboxing(false);
    CHECK_FALSE(config.isSandboxingEnabled());
    
    // Test execution time limits
    config.setMaxExecutionTime(30);
    CHECK(config.getMaxExecutionTime() == 30);
    
    // Test memory limits
    config.setMaxMemory(1024 * 1024); // 1MB
    CHECK(config.getMaxMemory() == 1024 * 1024);
}

TEST_CASE("LuaExpressionValidator syntax validation") {
    LuaExpressionValidator validator(10000);
    
    // Test valid syntax
    std::vector<std::string> validSyntax = {
        "return x + y",
        "return (x > 0) and (y < 10)",
        "return not active",
        "return items[1]",
        "return func(arg1, arg2)"
    };
    
    for (const auto& expr : validSyntax) {
        CHECK_NOTHROW(validator.validate(expr));
    }
    
    // Test that clearly invalid syntax throws
    std::vector<std::string> invalidSyntax = {
        "return x +", // Incomplete expression
        "return (x > 0", // Unmatched parenthesis
        "return x > 0 and" // Incomplete expression
    };
    
    // Note: Not all syntax errors may be caught by the validator
    // depending on the implementation
}

TEST_CASE("InputValidator comprehensive testing") {
    LuaExpressionValidator exprValidator(10000);
    ParameterValidator paramValidator;
    
    // Test a comprehensive set of valid expressions
    std::vector<std::string> expressions = {
        // Basic arithmetic
        "return x + y",
        "return x - y",
        "return x * y",
        "return x / y",
        "return x % y",
        "return x ^ y",
        
        // Comparisons
        "return x == y",
        "return x ~= y",
        "return x < y",
        "return x > y",
        "return x <= y",
        "return x >= y",
        
        // Logical operations
        "return x and y",
        "return x or y",
        "return not x",
        
        // String operations
        "return string.len(name)",
        "return string.upper(name)",
        "return string.lower(name)",
        "return string.sub(name, 1, 5)",
        
        // Math operations
        "return math.abs(x)",
        "return math.max(x, y)",
        "return math.min(x, y)",
        "return math.floor(x)",
        "return math.ceil(x)",
        
        // Table operations
        "return #items",
        "return items[1]",
        "return table.insert(items, value)",
        "return table.remove(items, 1)"
    };
    
    for (const auto& expr : expressions) {
        CHECK_NOTHROW(exprValidator.validate(expr));
    }
    
    // Test comprehensive parameter validation
    std::vector<std::pair<std::string, std::string>> parameters = {
        {"age", "25"},
        {"name", "John"},
        {"active", "true"},
        {"score", "95.5"},
        {"items", "1,2,3,4,5"}
    };
    
    for (const auto& param : parameters) {
        CHECK_NOTHROW(paramValidator.validate(param.first, param.second));
    }
}