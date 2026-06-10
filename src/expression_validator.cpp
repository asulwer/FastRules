#include "fastrules/expression_validator.hpp"
#include "fastrules/lua_engine.hpp"
#include <algorithm>
#include <cctype>
#include <sstream>

namespace fastrules {

bool ExpressionValidator::isValidLua(const std::string& expression, LuaEngine& engine) {
    if (expression.empty()) return true;
    
    // First do quick syntax check
    auto result = validate(expression);
    if (!result.valid) return false;
    
    // Then try actual compilation via sol2
    try {
        auto ref = engine.compileExpression(expression);
        if (ref.has_value()) {
            engine.releaseRef(ref.value());
            return true;
        }
        return false;
    } catch (const std::exception& /*e*/) {
        // Compilation failed - log at debug level if logger available
        if (engine.hasLogger()) {
            auto log = engine.getLogger();
            log->debug("Expression compilation failed: {}", expression);
        }
        return false;
    }
}

ExpressionValidator::ValidationResult ExpressionValidator::validate(const std::string& expression) {
    ValidationResult result;

    if (expression.empty()) {
        result.valid = true;  // Empty expressions are valid (return true)
        return result;
    }

    // Check for dangerous patterns
    auto dangerous = findDangerousPatterns(expression);
    if (!dangerous.empty()) {
        result.valid = false;
        for (const auto& pattern : dangerous) {
            result.errors.push_back("Dangerous pattern detected: " + pattern);
        }
    }

    // Check for common issues
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

    // Check for unclosed strings
    bool inString = false;
    char stringDelim = 0;
    for (size_t i = 0; i < expression.size(); ++i) {
        char c = expression[i];
        if (!inString && (c == '"' || c == '\'')) {
            inString = true;
            stringDelim = c;
        } else if (inString && c == stringDelim) {
            // Check if escaped
            if (i == 0 || expression[i-1] != '\\') {
                inString = false;
            }
        }
    }
    if (inString) {
        result.valid = false;
        result.errors.push_back("Unclosed string literal");
    }

    // Warnings
    if (expression.find("while true") != std::string::npos) {
        result.warnings.push_back("Expression contains 'while true' - potential infinite loop");
    }
    if (expression.find("for ") != std::string::npos && expression.find("do") != std::string::npos) {
        result.warnings.push_back("Expression contains loop - ensure timeout is set");
    }

    return result;
}

bool ExpressionValidator::containsDangerousPatterns(const std::string& expression) {
    return !findDangerousPatterns(expression).empty();
}

std::vector<std::string> ExpressionValidator::findDangerousPatterns(const std::string& expression) {
    std::vector<std::string> found;
    auto patterns = getDangerousPatterns();

    std::string lowerExpr = expression;
    std::transform(lowerExpr.begin(), lowerExpr.end(), lowerExpr.begin(), ::tolower);

    for (const auto& [pattern, description] : patterns) {
        if (lowerExpr.find(pattern) != std::string::npos) {
            found.push_back(description);
        }
    }

    return found;
}

bool ExpressionValidator::isValidLuaSyntax(const std::string& expression) {
    if (expression.empty()) return true;

    auto result = validate(expression);
    return result.valid;
}

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
