#include "fastrules/input_validator.hpp"
#include "fastrules/logger.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace fastrules {

LuaExpressionValidator::LuaExpressionValidator(size_t maxLength)
    : identifierPattern_(R"([a-zA-Z_][a-zA-Z0-9_]*)")
    , numberPattern_(R"(\d+\.?\d*)")
    , stringPattern_(R"(["'][^"']*["'])")
    , maxLength_(maxLength) {
    
    // Initialize allowed functions
    allowedFunctions_ = {
        "math.abs", "math.acos", "math.asin", "math.atan", "math.atan2",
        "math.ceil", "math.cos", "math.cosh", "math.deg", "math.exp",
        "math.floor", "math.fmod", "math.frexp", "math.ldexp", "math.log",
        "math.log10", "math.max", "math.min", "math.modf", "math.pow",
        "math.rad", "math.random", "math.sin", "math.sinh", "math.sqrt",
        "math.tan", "math.tanh",
        "string.byte", "string.char", "string.find", "string.format",
        "string.gmatch", "string.gsub", "string.len", "string.lower",
        "string.match", "string.rep", "string.reverse", "string.sub",
        "string.upper",
        "table.concat", "table.insert", "table.remove", "table.sort",
        "tonumber", "tostring", "type", "next", "ipairs", "pairs"
    };
    
    // Initialize allowed operators
    allowedOperators_ = {
        "+", "-", "*", "/", "%", "^", "#",
        "==", "~=", "<=", ">=", "<", ">",
        "and", "or", "not",
        "=", "(", ")", "{", "}", "[", "]",
        ";", ":", ".", "..", "..."
    };
    
    // Initialize dangerous patterns
    dangerousPatterns_ = {
        "os\\.", "io\\.", "debug\\.", "load", "loadfile", "dofile",
        "require", "package\\.", "module", "setmetatable",
        "getmetatable", "rawget", "rawset", "rawequal",
        "collectgarbage", "gcinfo", "newproxy",
        "system", "exec", "spawn", "popen",
        "import", "using", "include", "source",
        "eval", "execute", "run", "call"
    };
}

void LuaExpressionValidator::validate(const std::string& expression) const {
    // Check length
    if (expression.length() > maxLength_) {
        throw ValidationException("Expression too long (max " + std::to_string(maxLength_) + " characters)");
    }
    
    // Check for dangerous patterns
    checkDangerousPatterns(expression);
    
    // Validate syntax
    validateSyntax(expression);
    
    // Validate function calls
    validateFunctions(expression);
}

std::string LuaExpressionValidator::sanitize(const std::string& expression) const {
    std::string sanitized = expression;
    
    // Remove or escape dangerous characters
    // This is a simplified sanitization - in practice, you'd want more sophisticated cleaning
    sanitized.erase(std::remove(sanitized.begin(), sanitized.end(), '\0'), sanitized.end());
    
    // Limit length
    if (sanitized.length() > maxLength_) {
        sanitized.resize(maxLength_);
    }
    
    return sanitized;
}

bool LuaExpressionValidator::isFunctionAllowed(const std::string& functionName) const {
    return allowedFunctions_.find(functionName) != allowedFunctions_.end();
}

void LuaExpressionValidator::addAllowedFunction(const std::string& functionName) {
    allowedFunctions_.insert(functionName);
}

void LuaExpressionValidator::removeAllowedFunction(const std::string& functionName) {
    allowedFunctions_.erase(functionName);
}

void LuaExpressionValidator::setMaxLength(size_t maxLength) {
    maxLength_ = maxLength;
}

void LuaExpressionValidator::checkDangerousPatterns(const std::string& expression) const {
    std::string lowerExpr = expression;
    std::transform(lowerExpr.begin(), lowerExpr.end(), lowerExpr.begin(), ::tolower);
    
    for (const auto& pattern : dangerousPatterns_) {
        // Use simple string search instead of regex for better performance and reliability
        if (lowerExpr.find(pattern) != std::string::npos) {
            throw ValidationException("Dangerous pattern detected: " + pattern);
        }
    }
}

void LuaExpressionValidator::validateSyntax(const std::string& expression) const {
    // Basic syntax validation
    // This is a simplified validation - in practice, you'd want a proper parser
    
    // Check for balanced parentheses
    int parenCount = 0;
    int bracketCount = 0;
    int braceCount = 0;
    
    for (char c : expression) {
        switch (c) {
            case '(':
                parenCount++;
                break;
            case ')':
                parenCount--;
                if (parenCount < 0) {
                    throw ValidationException("Unbalanced parentheses");
                }
                break;
            case '[':
                bracketCount++;
                break;
            case ']':
                bracketCount--;
                if (bracketCount < 0) {
                    throw ValidationException("Unbalanced brackets");
                }
                break;
            case '{':
                braceCount++;
                break;
            case '}':
                braceCount--;
                if (braceCount < 0) {
                    throw ValidationException("Unbalanced braces");
                }
                break;
        }
    }
    
    if (parenCount != 0) {
        throw ValidationException("Unbalanced parentheses");
    }
    if (bracketCount != 0) {
        throw ValidationException("Unbalanced brackets");
    }
    if (braceCount != 0) {
        throw ValidationException("Unbalanced braces");
    }
}

void LuaExpressionValidator::validateFunctions(const std::string& expression) const {
    // This is a simplified function validation
    // In practice, you'd want a proper AST parser to identify function calls
    
    // For now, we'll just do basic pattern matching
    // A more sophisticated implementation would parse the expression
    // and check each function call against the whitelist
}

ParameterValidator::ParameterValidator()
    : maxParameterNameLength_(100)
    , maxParameterValueLength_(10000) {
    
    // Reserved parameter names that shouldn't be used
    reservedNames_ = {
        "nil", "true", "false", "function", "end", "if", "then", "else",
        "elseif", "while", "for", "in", "do", "repeat", "until", "break",
        "return", "local", "goto", "and", "or", "not", "self"
    };
}

void ParameterValidator::validateName(const std::string& name) const {
    if (name.empty()) {
        throw ValidationException("Parameter name cannot be empty");
    }
    
    if (name.length() > maxParameterNameLength_) {
        throw ValidationException("Parameter name too long");
    }
    
    // Check for reserved names
    std::string lowerName = name;
    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
    if (reservedNames_.find(lowerName) != reservedNames_.end()) {
        throw ValidationException("Parameter name is reserved: " + name);
    }
    
    // Check that name starts with a letter or underscore
    if (!std::isalpha(name[0]) && name[0] != '_') {
        throw ValidationException("Parameter name must start with a letter or underscore");
    }
    
    // Check that name contains only valid characters
    for (char c : name) {
        if (!std::isalnum(c) && c != '_') {
            throw ValidationException("Parameter name contains invalid characters");
        }
    }
}

void ParameterValidator::validateValue(const std::string& value) const {
    if (value.length() > maxParameterValueLength_) {
        throw ValidationException("Parameter value too long");
    }
    
    // Check for null bytes
    if (value.find('\0') != std::string::npos) {
        throw ValidationException("Parameter value contains null bytes");
    }
}

void ParameterValidator::validate(const std::string& name, const std::string& value) const {
    validateName(name);
    validateValue(value);
}

std::string ParameterValidator::sanitizeName(const std::string& name) const {
    std::string sanitized = name;
    
    // Remove invalid characters
    sanitized.erase(std::remove_if(sanitized.begin(), sanitized.end(), 
        [](char c) { return !std::isalnum(c) && c != '_'; }), sanitized.end());
    
    // Ensure it starts with a valid character
    if (!sanitized.empty() && !std::isalpha(sanitized[0]) && sanitized[0] != '_') {
        sanitized = "_" + sanitized;
    }
    
    // Limit length
    if (sanitized.length() > maxParameterNameLength_) {
        sanitized.resize(maxParameterNameLength_);
    }
    
    return sanitized;
}

std::string ParameterValidator::sanitizeValue(const std::string& value) const {
    std::string sanitized = value;
    
    // Remove null bytes
    sanitized.erase(std::remove(sanitized.begin(), sanitized.end(), '\0'), sanitized.end());
    
    // Limit length
    if (sanitized.length() > maxParameterValueLength_) {
        sanitized.resize(maxParameterValueLength_);
    }
    
    return sanitized;
}

// Static instance for SecurityConfig
static SecurityConfig* g_securityConfig = nullptr;

SecurityConfig::SecurityConfig()
    : expressionValidator_(std::make_unique<LuaExpressionValidator>())
    , parameterValidator_(std::make_unique<ParameterValidator>())
    , enableSandboxing_(true)
    , maxExecutionTimeSeconds_(30)
    , maxMemoryBytes_(1024 * 1024 * 100) {  // 100 MB default
}

LuaExpressionValidator& SecurityConfig::getExpressionValidator() {
    return *expressionValidator_;
}

ParameterValidator& SecurityConfig::getParameterValidator() {
    return *parameterValidator_;
}

void SecurityConfig::setSandboxing(bool enable) {
    enableSandboxing_ = enable;
}

bool SecurityConfig::isSandboxingEnabled() const {
    return enableSandboxing_;
}

void SecurityConfig::setMaxExecutionTime(size_t seconds) {
    maxExecutionTimeSeconds_ = seconds;
}

size_t SecurityConfig::getMaxExecutionTime() const {
    return maxExecutionTimeSeconds_;
}

void SecurityConfig::setMaxMemory(size_t bytes) {
    maxMemoryBytes_ = bytes;
}

size_t SecurityConfig::getMaxMemory() const {
    return maxMemoryBytes_;
}

SecurityConfig& getSecurityConfig() {
    if (!g_securityConfig) {
        g_securityConfig = new SecurityConfig();
    }
    return *g_securityConfig;
}

} // namespace fastrules