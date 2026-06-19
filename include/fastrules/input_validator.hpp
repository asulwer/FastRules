/**
 * @file input_validator.hpp
 * @brief Input validation for FastRules
 * 
 * Provides validation and sanitization for Lua expressions and other inputs
 * to prevent injection attacks and ensure security.
 */

#pragma once

#include <string>
#include <vector>
#include <unordered_set>
#include <regex>
#include <stdexcept>

namespace fastrules {

/**
 * @brief Exception thrown for invalid input
 */
class ValidationException : public std::runtime_error {
public:
    explicit ValidationException(const std::string& message) 
        : std::runtime_error(message) {}
};

/**
 * @brief Lua expression validator
 * 
 * Validates and sanitizes Lua expressions to prevent injection attacks
 * and ensure only allowed operations are performed.
 */
class LuaExpressionValidator {
private:
    // Whitelisted Lua functions and operators
    std::unordered_set<std::string> allowedFunctions_;
    std::unordered_set<std::string> allowedOperators_;
    std::unordered_set<std::string> dangerousPatterns_;
    
    // Regex patterns for validation
    std::regex identifierPattern_;
    std::regex numberPattern_;
    std::regex stringPattern_;
    
    // Maximum expression length
    size_t maxLength_;

public:
    /**
     * @brief Construct Lua expression validator
     * 
     * @param maxLength Maximum allowed expression length
     */
    explicit LuaExpressionValidator(size_t maxLength = 10000);

    /**
     * @brief Validate Lua expression
     * 
     * @param expression Expression to validate
     * @throws ValidationException if expression is invalid
     */
    void validate(const std::string& expression) const;

    /**
     * @brief Sanitize Lua expression
     * 
     * @param expression Expression to sanitize
     * @return Sanitized expression
     */
    std::string sanitize(const std::string& expression) const;

    /**
     * @brief Check if function is allowed
     * 
     * @param functionName Function name to check
     * @return true if function is allowed
     */
    bool isFunctionAllowed(const std::string& functionName) const;

    /**
     * @brief Add allowed function
     * 
     * @param functionName Function to allow
     */
    void addAllowedFunction(const std::string& functionName);

    /**
     * @brief Remove allowed function
     * 
     * @param functionName Function to disallow
     */
    void removeAllowedFunction(const std::string& functionName);

    /**
     * @brief Set maximum expression length
     * 
     * @param maxLength Maximum length
     */
    void setMaxLength(size_t maxLength);

private:
    /**
     * @brief Check for dangerous patterns
     * 
     * @param expression Expression to check
     * @throws ValidationException if dangerous patterns found
     */
    void checkDangerousPatterns(const std::string& expression) const;

    /**
     * @brief Validate syntax
     * 
     * @param expression Expression to validate
     * @throws ValidationException if syntax is invalid
     */
    void validateSyntax(const std::string& expression) const;

    /**
     * @brief Validate function calls
     * 
     * @param expression Expression to validate
     * @throws ValidationException if forbidden functions found
     */
    void validateFunctions(const std::string& expression) const;
};

/**
 * @brief Rule parameter validator
 * 
 * Validates rule parameters to prevent injection and ensure type safety.
 */
class ParameterValidator {
private:
    std::unordered_set<std::string> reservedNames_;
    size_t maxParameterNameLength_;
    size_t maxParameterValueLength_;

public:
    /**
     * @brief Construct parameter validator
     */
    ParameterValidator();

    /**
     * @brief Validate parameter name
     * 
     * @param name Parameter name to validate
     * @throws ValidationException if name is invalid
     */
    void validateName(const std::string& name) const;

    /**
     * @brief Validate parameter value
     * 
     * @param value Parameter value to validate
     * @throws ValidationException if value is invalid
     */
    void validateValue(const std::string& value) const;

    /**
     * @brief Validate parameter
     * 
     * @param name Parameter name
     * @param value Parameter value
     * @throws ValidationException if parameter is invalid
     */
    void validate(const std::string& name, const std::string& value) const;

    /**
     * @brief Sanitize parameter name
     * 
     * @param name Parameter name to sanitize
     * @return Sanitized name
     */
    std::string sanitizeName(const std::string& name) const;

    /**
     * @brief Sanitize parameter value
     * 
     * @param value Parameter value to sanitize
     * @return Sanitized value
     */
    std::string sanitizeValue(const std::string& value) const;
};

/**
 * @brief Security configuration
 * 
 * Centralized security configuration for FastRules.
 */
class SecurityConfig {
private:
    std::unique_ptr<LuaExpressionValidator> expressionValidator_;
    std::unique_ptr<ParameterValidator> parameterValidator_;
    bool enableSandboxing_;
    size_t maxExecutionTimeSeconds_;
    size_t maxMemoryBytes_;

public:
    /**
     * @brief Construct security configuration
     */
    SecurityConfig();

    /**
     * @brief Get expression validator
     * 
     * @return Expression validator
     */
    LuaExpressionValidator& getExpressionValidator();

    /**
     * @brief Get parameter validator
     * 
     * @return Parameter validator
     */
    ParameterValidator& getParameterValidator();

    /**
     * @brief Enable/disable sandboxing
     * 
     * @param enable true to enable sandboxing
     */
    void setSandboxing(bool enable);

    /**
     * @brief Check if sandboxing is enabled
     * 
     * @return true if sandboxing is enabled
     */
    bool isSandboxingEnabled() const;

    /**
     * @brief Set maximum execution time
     * 
     * @param seconds Maximum execution time in seconds
     */
    void setMaxExecutionTime(size_t seconds);

    /**
     * @brief Get maximum execution time
     * 
     * @return Maximum execution time in seconds
     */
    size_t getMaxExecutionTime() const;

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
};

/**
 * @brief Get global security configuration
 * 
 * @return Security configuration
 */
SecurityConfig& getSecurityConfig();

} // namespace fastrules