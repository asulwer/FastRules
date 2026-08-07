/**
 * @file parameter_validator.cpp
 * @brief Runtime parameter type validation
 * 
 * The ParameterValidator validates that parameter values match their
 * declared types at runtime. This catches type mismatches early before
 * they cause Lua runtime errors.
 * 
 * Supported Types:
 * - int/integer: Any integer type (int, long, long long, short)
 * - double/float/number: Any floating point type, or integer
 * - bool/boolean: Boolean values
 * - string/std::string/str: String values (std::string or const char*)
 * - array/list/vector: Container types (std::vector<T>)
 * - optional/nullable: Always valid (indicates nullable parameter)
 * 
 * Type Coercion:
 * - Integers are accepted for double parameters (numeric promotion)
 * - nullptr/empty std::any is valid for any type (Lua nil semantics)
 * 
 * Thread Safety:
 * - All methods are thread-safe (no mutable state)
 * - Safe to call from multiple threads simultaneously
 */

#include "fastrules/parameter_validator.hpp"
#include "fastrules/rule.hpp"
#include <any>
#include <sstream>

namespace fastrules {

/**
 * @brief Validate all parameters against their declared types
 * 
 * Iterates through all parameters and validates that each value
 * matches its declared type (if any). Throws ParameterTypeException
 * on the first validation failure.
 * 
 * @param parameters Vector of RuleParameter to validate
 * @throws ParameterTypeException if any parameter fails type validation
 */
void ParameterValidator::validateTypes(const std::vector<RuleParameter>& parameters) {
    for (const auto& param : parameters) {
        if (!param.type.has_value()) {
            continue;
        }
        // Compare std::type_index values directly rather than routing through
        // type_info::name(). That name is implementation-defined - MSVC yields
        // "int" but libstdc++ yields "i" - so the string-based check silently
        // degraded to a no-op on GCC/Clang and behaved differently per platform.
        if (!valueMatchesTypeIndex(param.type.value(), param.value)) {
            std::ostringstream oss;
            oss << "Parameter '" << param.name << "' declared as type '"
                << param.type.value().name()
                << "' but value holds '"
                << (param.value.has_value() ? param.value.type().name() : "nil")
                << "'";
            throw ParameterTypeException(oss.str());
        }
    }
}

bool ParameterValidator::valueMatchesTypeIndex(const std::type_index& declaredType,
                                               const std::any& value) {
    // nil/null matches any type (Lua nil is valid for any parameter)
    if (!value.has_value()) {
        return true;
    }

    // Exact match is the common case (RuleParameter records typeid(T) of the
    // value it was constructed from).
    if (std::type_index(value.type()) == declaredType) {
        return true;
    }

    // Only a scalar-vs-scalar mismatch is diagnosable here. Everything else is
    // the TypeRegistry's business - in particular pointer parameters, for which
    // RuleParameter deliberately records the POINTEE type, so a `Customer*`
    // value legitimately carries a declared type of `Customer`.
    const bool declaredScalar = isNumericTypeIndex(declaredType) ||
                                isStringTypeIndex(declaredType) ||
                                declaredType == std::type_index(typeid(bool));
    const bool valueScalar = isInt(value) || isDouble(value) || isBool(value) || isString(value);
    if (!declaredScalar || !valueScalar) {
        return true;
    }

    if (declaredType == std::type_index(typeid(bool))) {
        return isBool(value);
    }
    if (isStringTypeIndex(declaredType)) {
        return isString(value);
    }
    // Declared numeric: accept any numeric value (Lua performs the conversion
    // anyway), including bool, which Lua treats as truthy/falsy.
    return isInt(value) || isDouble(value) || isBool(value);
}

bool ParameterValidator::isStringTypeIndex(const std::type_index& t) {
    return t == std::type_index(typeid(std::string)) ||
           t == std::type_index(typeid(const char*)) ||
           t == std::type_index(typeid(char*));
}

bool ParameterValidator::isNumericTypeIndex(const std::type_index& t) {
    return t == std::type_index(typeid(int)) ||
           t == std::type_index(typeid(unsigned int)) ||
           t == std::type_index(typeid(short)) ||
           t == std::type_index(typeid(unsigned short)) ||
           t == std::type_index(typeid(long)) ||
           t == std::type_index(typeid(unsigned long)) ||
           t == std::type_index(typeid(long long)) ||
           t == std::type_index(typeid(unsigned long long)) ||
           t == std::type_index(typeid(float)) ||
           t == std::type_index(typeid(double)) ||
           t == std::type_index(typeid(long double));
}

/**
 * @brief Check if a value matches a declared type
 * 
 * Performs runtime type checking using std::type_info comparison.
 * Supports various aliases for the same underlying type.
 * 
 * Special cases:
 * - Empty std::any (no value) matches any type (Lua nil semantics)
 * - Integers match double type (numeric promotion)
 * - optional/nullable types always match
 * 
 * @param declaredType The expected type name
 * @param value The value to check
 * @return true if value matches declaredType, false otherwise
 */
bool ParameterValidator::valueMatchesType(const std::string& declaredType, const std::any& value) {
    // nil/null matches any type (Lua nil is valid for any parameter)
    if (!value.has_value()) {
        return true;
    }
    
    // Check against supported type categories
    if (declaredType == "int" || declaredType == "integer") {
        return isInt(value);
    } else if (declaredType == "long" || declaredType == "long long" || declaredType == "int64") {
        return value.type() == typeid(long) || value.type() == typeid(long long);
    } else if (declaredType == "double" || declaredType == "float" || declaredType == "number") {
        // Integers are valid for double parameters (numeric promotion)
        return isDouble(value) || isInt(value);
    } else if (declaredType == "bool" || declaredType == "boolean") {
        return isBool(value);
    } else if (declaredType == "string" || declaredType == "std::string" || declaredType == "str") {
        return isString(value);
    } else if (declaredType == "array" || declaredType == "list" || declaredType == "vector") {
        // Check for common container types
        return value.type() == typeid(std::vector<int>) ||
               value.type() == typeid(std::vector<double>) ||
               value.type() == typeid(std::vector<std::string>);
    } else if (declaredType == "optional" || declaredType == "nullable") {
        // std::optional<T> or empty std::any
        // Always valid - optional means it CAN be null
        return true;
    }
    
    // Unknown type - allow it (custom types handled separately by TypeRegistry)
    // This prevents false negatives for user-defined types
    return true;
}

/**
 * @brief Get list of supported type names
 * 
 * Returns all type names that are recognized by the validator.
 * Useful for documentation and error messages.
 * 
 * @return Vector of supported type name strings
 */
std::vector<std::string> ParameterValidator::getSupportedTypes() {
    return {
        "int", "integer", "long", "long long", "int64",
        "double", "float", "number",
        "bool", "boolean",
        "string", "std::string", "str",
        "array", "list", "vector",
        "optional", "nullable"
    };
}

// ============================================================================
// Type Check Helpers
// ============================================================================

/**
 * @brief Check if value is any integer type
 * 
 * Recognizes: int, long, long long, short
 * 
 * @param value The std::any value to check
 * @return true if value contains an integer type
 */
bool ParameterValidator::isInt(const std::any& value) {
    return value.type() == typeid(int) ||
           value.type() == typeid(long) ||
           value.type() == typeid(long long) ||
           value.type() == typeid(short);
}

/**
 * @brief Check if value is any floating point type
 * 
 * Recognizes: double, float, long double
 * 
 * @param value The std::any value to check
 * @return true if value contains a floating point type
 */
bool ParameterValidator::isDouble(const std::any& value) {
    return value.type() == typeid(double) ||
           value.type() == typeid(float) ||
           value.type() == typeid(long double);
}

/**
 * @brief Check if value is a boolean
 * 
 * @param value The std::any value to check
 * @return true if value contains a bool
 */
bool ParameterValidator::isBool(const std::any& value) {
    return value.type() == typeid(bool);
}

/**
 * @brief Check if value is a string type
 * 
 * Recognizes: std::string, const char*
 * 
 * @param value The std::any value to check
 * @return true if value contains a string type
 */
bool ParameterValidator::isString(const std::any& value) {
    return value.type() == typeid(std::string) ||
           value.type() == typeid(const char*);
}

} // namespace fastrules
