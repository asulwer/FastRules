#include "fastrules/input_validator.hpp"
#include <doctest/doctest.h>

TEST_CASE("LuaExpressionValidator basic functionality") {
    fastrules::LuaExpressionValidator validator(1000);
    
    // Test valid expressions
    CHECK_NOTHROW(validator.validate("return 1 + 1"));
    CHECK_NOTHROW(validator.validate("return x > 10"));
    CHECK_NOTHROW(validator.validate("return math.sqrt(value)"));
    CHECK_NOTHROW(validator.validate("return string.len(text)"));
    
    // Test invalid expressions - dangerous patterns
    CHECK_THROWS_AS(validator.validate("return os.execute('ls')"), fastrules::ValidationException);
    CHECK_THROWS_AS(validator.validate("return io.open('file.txt')"), fastrules::ValidationException);
    CHECK_THROWS_AS(validator.validate("return load('code')"), fastrules::ValidationException);
    CHECK_THROWS_AS(validator.validate("return debug.traceback()"), fastrules::ValidationException);
    
    // Test invalid expressions - length
    std::string longExpr(1001, 'a');
    CHECK_THROWS_AS(validator.validate(longExpr), fastrules::ValidationException);
    
    // Test invalid expressions - syntax
    CHECK_THROWS_AS(validator.validate("return (1 + 2"), fastrules::ValidationException);
    CHECK_THROWS_AS(validator.validate("return [1 + 2"), fastrules::ValidationException);
    CHECK_THROWS_AS(validator.validate("return {1 + 2"), fastrules::ValidationException);
}

TEST_CASE("LuaExpressionValidator function validation") {
    fastrules::LuaExpressionValidator validator(1000);
    
    // Test allowed functions
    CHECK(validator.isFunctionAllowed("math.abs"));
    CHECK(validator.isFunctionAllowed("string.len"));
    CHECK(validator.isFunctionAllowed("table.insert"));
    
    // Test adding and removing functions
    CHECK_FALSE(validator.isFunctionAllowed("custom.function"));
    validator.addAllowedFunction("custom.function");
    CHECK(validator.isFunctionAllowed("custom.function"));
    validator.removeAllowedFunction("custom.function");
    CHECK_FALSE(validator.isFunctionAllowed("custom.function"));
}

TEST_CASE("LuaExpressionValidator sanitization") {
    fastrules::LuaExpressionValidator validator(1000);
    
    // Test sanitization
    std::string cleanExpr = "return 1 + 1";
    std::string sanitized = validator.sanitize(cleanExpr);
    CHECK(sanitized == cleanExpr);
    
    // Test length limiting
    std::string longExpr(1500, 'a');
    std::string sanitizedLong = validator.sanitize(longExpr);
    CHECK(sanitizedLong.length() <= 1000);
}

TEST_CASE("ParameterValidator basic functionality") {
    fastrules::ParameterValidator validator;
    
    // Test valid parameter names
    CHECK_NOTHROW(validator.validateName("value"));
    CHECK_NOTHROW(validator.validateName("test_param"));
    CHECK_NOTHROW(validator.validateName("_private"));
    
    // Test invalid parameter names
    CHECK_THROWS_AS(validator.validateName(""), fastrules::ValidationException);
    CHECK_THROWS_AS(validator.validateName("123invalid"), fastrules::ValidationException);
    CHECK_THROWS_AS(validator.validateName("invalid-name"), fastrules::ValidationException);
    CHECK_THROWS_AS(validator.validateName("function"), fastrules::ValidationException);
    
    // Test valid parameter values
    CHECK_NOTHROW(validator.validateValue("test value"));
    CHECK_NOTHROW(validator.validateValue("123"));
    
    // Test invalid parameter values - length
    std::string longValue(10001, 'a');
    CHECK_THROWS_AS(validator.validateValue(longValue), fastrules::ValidationException);
    
    // Test invalid parameter values - null bytes
    std::string nullValue = std::string("test") + '\0' + "value";
    CHECK_THROWS_AS(validator.validateValue(nullValue), fastrules::ValidationException);
}

TEST_CASE("ParameterValidator sanitization") {
    fastrules::ParameterValidator validator;
    
    // Test name sanitization
    std::string cleanName = "valid_name";
    std::string sanitized = validator.sanitizeName(cleanName);
    CHECK(sanitized == cleanName);
    
    // Test invalid name sanitization
    std::string invalidName = "123-invalid";
    std::string sanitizedInvalid = validator.sanitizeName(invalidName);
    CHECK(sanitizedInvalid == "_123invalid");
    
    // Test value sanitization
    std::string cleanValue = "valid value";
    std::string sanitizedValue = validator.sanitizeValue(cleanValue);
    CHECK(sanitizedValue == cleanValue);
    
    // Test null byte removal
    std::string nullValue = std::string("test") + '\0' + "value";
    std::string sanitizedNull = validator.sanitizeValue(nullValue);
    CHECK(sanitizedNull == "testvalue");
}

TEST_CASE("SecurityConfig basic functionality") {
    auto& config = fastrules::getSecurityConfig();
    
    // Test default values
    CHECK(config.isSandboxingEnabled());
    CHECK(config.getMaxExecutionTime() == 30);
    CHECK(config.getMaxMemory() == 1024 * 1024 * 100);
    
    // Test setting values
    config.setSandboxing(false);
    config.setMaxExecutionTime(60);
    config.setMaxMemory(1024 * 1024 * 200);
    
    CHECK_FALSE(config.isSandboxingEnabled());
    CHECK(config.getMaxExecutionTime() == 60);
    CHECK(config.getMaxMemory() == 1024 * 1024 * 200);
    
    // Test validators access
    auto& exprValidator = config.getExpressionValidator();
    auto& paramValidator = config.getParameterValidator();
    
    CHECK_NOTHROW(exprValidator.validate("return 1 + 1"));
    CHECK_NOTHROW(paramValidator.validateName("test"));
}