#pragma once

#include <string>
#include <vector>
#include <stdexcept>

namespace fastrules {

// Thrown when an expression contains dangerous patterns
class ExpressionValidationException : public std::runtime_error {
public:
    ExpressionValidationException(const std::string& msg) : std::runtime_error(msg) {}
};

// Pre-validates Lua expressions for dangerous patterns before compilation
class ExpressionValidator {
public:
    struct ValidationResult {
        bool valid = true;
        std::vector<std::string> warnings;
        std::vector<std::string> errors;
    };

    // Full validation: syntax check + dangerous pattern detection
    static ValidationResult validate(const std::string& expression);

    // Check for dangerous patterns that could compromise security
    static bool containsDangerousPatterns(const std::string& expression);

    // Get list of dangerous patterns found
    static std::vector<std::string> findDangerousPatterns(const std::string& expression);

    // Quick syntax check (doesn't catch everything, but fast)
    static bool isValidLuaSyntax(const std::string& expression);

    // Deep validation: attempts a real Lua compilation via sol2
    // Returns true if expression compiles successfully
    static bool isValidLua(const std::string& expression, class LuaEngine& engine);

private:
    static std::vector<std::pair<std::string, std::string>> getDangerousPatterns();
};

} // namespace fastrules
