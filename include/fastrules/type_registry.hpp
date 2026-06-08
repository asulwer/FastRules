#pragma once

#include <string>
#include <vector>
#include <any>
#include <typeindex>
#include <optional>
#include <unordered_map>
#include <unordered_set>

namespace fastrules {

// ============================================================================
// Backend-neutral type field descriptor
// ============================================================================
struct TypeField {
    std::string name;
    size_t offset;
    std::string luaType; // "int", "double", "bool", "string"
};

// ============================================================================
// Backend-neutral type descriptor
// Stored by TypeRegistry, consumed by backends to create native bindings
// ============================================================================
struct TypeDescriptor {
    std::string name;
    std::type_index type;
    std::vector<TypeField> fields;
    size_t size = 0;
};

// ============================================================================
// Type Registry — backend-neutral storage for C++ type descriptors
//
// Backends (Sol2Backend, LuaBridge3Backend) read descriptors and create
// their own native bindings. LuaEngine owns a TypeRegistry and passes it
// to the backend via bindTypes().
// ============================================================================
class TypeRegistry {
public:
    TypeRegistry() = default;

    template<typename T>
    void registerType(const std::string& name, std::vector<TypeField> fields) {
        TypeDescriptor desc;
        desc.name = name;
        desc.type = std::type_index(typeid(T));
        desc.fields = std::move(fields);
        desc.size = sizeof(T);
        types_[desc.type] = std::move(desc);
        types_[std::type_index(typeid(T*))] = types_[desc.type]; // pointer alias
        nameToType_[name] = desc.type;
    }

    [[nodiscard]] bool isRegistered(const std::type_index& type) const {
        return types_.contains(type);
    }

    [[nodiscard]] bool isRegistered(const std::string& name) const {
        auto it = nameToType_.find(name);
        return it != nameToType_.end();
    }

    [[nodiscard]] std::optional<TypeDescriptor> getDescriptor(const std::type_index& type) const {
        auto it = types_.find(type);
        if (it != types_.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    [[nodiscard]] std::optional<TypeDescriptor> getDescriptor(const std::string& name) const {
        auto it = nameToType_.find(name);
        if (it != nameToType_.end()) {
            return getDescriptor(it->second);
        }
        return std::nullopt;
    }

    [[nodiscard]] const std::unordered_map<std::type_index, TypeDescriptor>& allTypes() const {
        return types_;
    }

private:
    std::unordered_map<std::type_index, TypeDescriptor> types_;
    std::unordered_map<std::string, std::type_index> nameToType_;
};

} // namespace fastrules
