#include "fastrules/parameter_validator.hpp"
#include "fastrules/rule.hpp"
#include <any>
#include <sstream>

namespace fastrules {

void ParameterValidator::validateTypes(const std::vector<RuleParameter>& parameters) {
    for (const auto& param : parameters) {
        if (!valueMatchesType(param.type, param.value)) {
            std::ostringstream oss;
            oss << "Parameter '" << param.name << "' declared as type '" << param.type
                << "' but value does not match";
            throw ParameterTypeException(oss.str());
        }
    }
}

bool ParameterValidator::valueMatchesType(const std::string& declaredType, const std::any& value) {
    if (!value.has_value()) {
        // nil/null matches any type (Lua nil is valid for any parameter)
        return true;
    }
    
    if (declaredType == "int" || declaredType == "integer") {
        return isInt(value);
    } else if (declaredType == "long" || declaredType == "long long" || declaredType == "int64") {
        return value.type() == typeid(long) || value.type() == typeid(long long);
    } else if (declaredType == "double" || declaredType == "float" || declaredType == "number") {
        return isDouble(value) || isInt(value);  // Ints are valid doubles
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
        return true; // Always valid - optional means it CAN be null
    }
    // Unknown type - allow it (custom types handled separately)
    return true;
}

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

bool ParameterValidator::isInt(const std::any& value) {
    return value.type() == typeid(int) ||
           value.type() == typeid(long) ||
           value.type() == typeid(long long) ||
           value.type() == typeid(short);
}

bool ParameterValidator::isDouble(const std::any& value) {
    return value.type() == typeid(double) ||
           value.type() == typeid(float) ||
           value.type() == typeid(long double);
}

bool ParameterValidator::isBool(const std::any& value) {
    return value.type() == typeid(bool);
}

bool ParameterValidator::isString(const std::any& value) {
    return value.type() == typeid(std::string) ||
           value.type() == typeid(const char*);
}

} // namespace fastrules
