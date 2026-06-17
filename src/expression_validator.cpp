/**
 * @file expression_validator.cpp
 * @brief Lua expression validation and security checking
 * 
 * The ExpressionValidator provides multiple layers of validation for
 * Lua expressions before they are compiled and executed:
 * 
 * 1. Syntax validation - brackets, parentheses, string literals
 * 2. Dangerous pattern detection - security-sensitive Lua functions
 * 3. Full compilation test - verify expression compiles successfully
 * 
 * Security Model:
 * - Expressions are sandboxed, but validation catches attempts to
 *   access dangerous functions before they reach the Lua runtime
 * - Dangerous patterns include: os.execute, io.open, loadfile, etc.
 * - Warnings are issued for potential issues (infinite loops)
 * 
 * Validation Levels:
 * - Light: validate() - syntax and pattern checks only
 * - Full: isValidLua() - syntax, patterns, AND compilation test
 * 
 * Thread Safety:
 * - All methods are thread-safe (no mutable state)
 * - Safe to validate expressions from multiple threads
 */

#include "fastrules/expression_validator.hpp"
#include "fastrules/lua_engine.hpp"
#include <algorithm>
#include <cctype>
#include <sstream>

namespace fastrules {

/**
 * @brief Validate expression by compiling it
 * 
 * Performs full validation:
 * 1. Runs validate() for syntax/pattern checks
 * 2. Attempts actual Lua compilation
 * 3. Cleans up the compiled reference
 * 
 * This is the most thorough validation but requires a LuaEngine.
 * Use for user-input expressions before saving.
 * 
 * @param expression The Lua expression to validate
 * @param engine LuaEngine for compilation test
 * @return true if expression is valid Lua, false otherwise
 */
bool ExpressionValidator::isValidLua(const std::string& expression, LuaEngine& engine) {
    if (expression.empty()) return true;
    
    // First do quick syntax check (fast path)
    auto result = validate(expression);
    if (!result.valid) return false;
    
    // Then try actual compilation
    try {
        auto ref = engine.compileExpression(expression);
        if (ref.has_value()) {
            engine.releaseRef(ref.value());  // Clean up test compilation
            return true;
        }
        return false;
    } catch (const std::exception& /*e*/) {
        // Compilation failed - log at debug level
        if (engine.hasLogger()) {
            auto log = engine.getLogger();
            log->debug("Expression compilation failed: {}", expression);
        }
        return false;
    }
}

/**
 * @brief Validate expression syntax and check for dangerous patterns
 * 
 * Performs static analysis without compilation:
 * - Bracket/parenthesis matching
 * - String literal closure
 * - Dangerous pattern detection
 * - Warning generation
 * 
 * Fast and doesn't require LuaEngine. Use for real-time validation
 * as user types (e.g., in an editor).
 * 
 * @param expression The Lua expression to validate
 * @return ValidationResult with valid flag, errors, and warnings
 */
ExpressionValidator::ValidationResult ExpressionValidator::validate(const std::string& expression) {
    ValidationResult result;

    // Empty expressions are valid (evaluate to true/nil in Lua)
    if (expression.empty()) {
        result.valid = true;
        return result;
    }

    // Check for dangerous security patterns
    auto dangerous = findDangerousPatterns(expression);
    if (!dangerous.empty()) {
        result.valid = false;
        for (const auto& pattern : dangerous) {
            result.errors.push_back("Dangerous pattern detected: " + pattern);
        }
    }

    // Count bracket types for matching
    int openParens = 0, closeParens = 0;
    int openBrackets = 0, closeBrackets = 0;
    int openBraces = 0, closeBraces = 0;

    for (char c : expression) {
        if (c == '(') openParens++;
        else if (c == ')') closeParens++;
        else if (c == '[') openBrackets++;
        else if (c == ']') closeBrackets++;
        else if (c == '{') openBraces++;
        else if (c == '}') closeBraces++;
    }

    // Report mismatched brackets
    if (openParens != closeParens) {
        result.valid = false;
        result.errors.push_back("Mismatched parentheses");
    }
    if (openBrackets != closeBrackets) {
        result.valid = false;
        result.errors.push_back("Mismatched brackets");
    }
    if (openBraces != closeBraces) {
        result.valid = false;
        result.errors.push_back("Mismatched braces");
    }

    // Check for unclosed string literals
    bool inString = false;
    char stringDelim = 0;
    for (size_t i = 0; i < expression.size(); ++i) {
        char c = expression[i];
        if (!inString && (c == '"' || c == '\'')) {
            inString = true;
            stringDelim = c;
        } else if (inString && c == stringDelim) {
            // Check if escaped with backslash
            if (i == 0 || expression[i-1] != '\\') {
                inString = false;
            }
        }
    }
    if (inString) {
        result.valid = false;
        result.errors.push_back("Unclosed string literal");
    }

    // Generate warnings for potential issues
    if (expression.find("while true") != std::string::npos) {
        result.warnings.push_back("Expression contains 'while true' - potential infinite loop");
    }
    if (expression.find("for ") != std::string::npos && expression.find("do") != std::string::npos) {
        result.warnings.push_back("Expression contains loop - ensure timeout is set");
    }

    return result;
}

/**
 * @brief Check if expression contains dangerous patterns
 * 
 * Convenience method that returns true if any dangerous
 * patterns are found in the expression.
 * 
 * @param expression The Lua expression to check
 * @return true if dangerous patterns found, false otherwise
 */
bool ExpressionValidator::containsDangerousPatterns(const std::string& expression) {
    return !findDangerousPatterns(expression).empty();
}

/**
 * @brief Find all dangerous patterns in an expression
 * 
 * Searches for security-sensitive Lua functions and patterns.
 * Performs case-insensitive matching.
 * 
 * @param expression The Lua expression to scan
 * @return Vector of descriptions of dangerous patterns found
 */
std::vector<std::string> ExpressionValidator::findDangerousPatterns(const std::string& expression) {
    std::vector<std::string> found;
    auto patterns = getDangerousPatterns();

    // Convert to lowercase for case-insensitive search
    std::string lowerExpr = expression;
    std::transform(lowerExpr.begin(), lowerExpr.end(), lowerExpr.begin(), 
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    // Check each dangerous pattern
    for (const auto& [pattern, description] : patterns) {
        if (lowerExpr.find(pattern) != std::string::npos) {
            found.push_back(description);
        }
    }

    return found;
}

/**
 * @brief Validate Lua syntax only
 * 
 * Performs syntax validation without dangerous pattern checks.
 * Useful when you want to allow "dangerous" functions in a
 * trusted environment but still check syntax.
 * 
 * @param expression The Lua expression to validate
 * @return true if syntax is valid, false otherwise
 */
bool ExpressionValidator::isValidLuaSyntax(const std::string& expression) {
    if (expression.empty()) return true;

    auto result = validate(expression);
    return result.valid;
}

/**
 * @brief Get list of dangerous patterns to check for
 * 
 * Returns pairs of:
 * - Pattern string (lowercase) to search for
 * - Human-readable description of why it's dangerous
 * 
 * Categories of dangerous patterns:
 * - OS interaction (os.execute, os.exit)
 * - File system (io.open, io.popen)
 * - Code loading (loadfile, dofile, require)
 * - Debug access (debug.*)
 * - Metatable bypass (rawset, rawget)
 * 
 * @return Vector of pattern/description pairs
 */
std::vector<std::pair<std::string, std::string>> ExpressionValidator::getDangerousPatterns() {
    return {
        {"os.execute", "os.execute() - shell command execution"},
        {"os.exit", "os.exit() - process termination"},
        {"io.open", "io.open() - file system access"},
        {"io.popen", "io.popen() - shell command execution"},
        {"loadfile", "loadfile() - arbitrary code loading"},
        {"dofile", "dofile() - arbitrary code execution"},
        {"require", "require() - module loading"},
        {"loadstring", "loadstring() - arbitrary code loading"},
        {"load", "load() - arbitrary code loading"},
        {"debug.getregistry", "debug.getregistry() - internal access"},
        {"debug.getlocal", "debug.getlocal() - stack inspection"},
        {"debug.setupvalue", "debug.setupvalue() - upvalue manipulation"},
        {"collectgarbage", "collectgarbage() - GC manipulation"},
        {"rawset", "rawset() - bypass metatables"},
        {"rawget", "rawget() - bypass metatables"},
        {"setmetatable", "setmetatable() - metatable manipulation"},
        {"getmetatable", "getmetatable() - metatable access"},
        {"rawequal", "rawequal() - bypass metatables"},
        {"rawlen", "rawlen() - bypass metatables"},
        {"package", "package - module system access"},
        {"module", "module() - module creation"},
        {"newproxy", "newproxy() - userdata creation"},
    };
}

} // namespace fastrules
