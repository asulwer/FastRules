#pragma once

#include <string>
#include <vector>
#include <any>
#include <stdexcept>
#include <typeindex>

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

    // Check if a parameter value matches its declared type, by type name.
    // Kept for callers that only have a name string; note that names such as
    // "int"/"string" are the FastRules vocabulary, NOT type_info::name().
    static bool valueMatchesType(const std::string& declaredType, const std::any& value);

    // Check a value against a declared type_index. Preferred over the
    // name-based overload: type_info::name() is implementation-defined, so
    // comparing against it gives different results on MSVC and GCC/Clang.
    static bool valueMatchesTypeIndex(const std::type_index& declaredType, const std::any& value);

    // Get supported type names
    static std::vector<std::string> getSupportedTypes();

private:
    static bool isNumericTypeIndex(const std::type_index& t);
    static bool isStringTypeIndex(const std::type_index& t);
    static bool isInt(const std::any& value);
    static bool isDouble(const std::any& value);
    static bool isBool(const std::any& value);
    static bool isString(const std::any& value);
};

} // namespace fastrules
