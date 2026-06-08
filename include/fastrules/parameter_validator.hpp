#pragma once

#include <string>
#include <vector>
#include <any>
#include <stdexcept>

namespace fastrules {

struct RuleParameter;

// Thrown when parameter types don't match expected types
class ParameterTypeException : public std::runtime_error {
public:
    ParameterTypeException(const std::string& msg) : std::runtime_error(msg) {}
};

// Validates parameters before rule execution
class ParameterValidator {
public:
    // Validate that all parameters are of supported types
    static void validateTypes(const std::vector<RuleParameter>& parameters);

    // Check if a parameter value matches its declared type
    static bool valueMatchesType(const std::string& declaredType, const std::any& value);

    // Get supported type names
    static std::vector<std::string> getSupportedTypes();

private:
    static bool isInt(const std::any& value);
    static bool isDouble(const std::any& value);
    static bool isBool(const std::any& value);
    static bool isString(const std::any& value);
};

} // namespace fastrules
