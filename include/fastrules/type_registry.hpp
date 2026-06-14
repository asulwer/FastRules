#pragma once

#include <string>
#include <vector>
#include <any>
#include <typeindex>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <functional>

// Forward declaration for TypeMethod invoker
struct lua_State;

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
// Backend-neutral type method descriptor
// ============================================================================
struct TypeMethod {
    std::string name;
    // Type-erased function that calls the method on a given object
    // Returns number of values pushed to Lua stack
    std::function<int(void*, lua_State*)> invoker;
};

// ============================================================================
// Backend-neutral type descriptor
// Stored by TypeRegistry, consumed by backends to create native bindings
// ============================================================================
struct TypeDescriptor {
    std::string name;
    std::optional<std::type_index> type;
    std::vector<TypeField> fields;
    std::vector<TypeMethod> methods;
    size_t size = 0;
    // Type-erased function to extract void* pointer from std::any containing T*
    std::function<void*(const std::any&)> extractPointer;
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
    void registerType(const std::string& name, std::vector<TypeField> fields, std::vector<TypeMethod> methods = {}) {
        TypeDescriptor desc;
        
        // If type already registered, merge with existing
        auto existing = types_.find(std::type_index(typeid(T)));
        if (existing != types_.end()) {
            desc = existing->second;
        }
        
        desc.name = name;
        auto t = std::type_index(typeid(T));
        desc.type = t;
        
        // Merge fields (append new ones)
        for (auto& f : fields) {
            // Check if field already exists
            bool found = false;
            for (auto& ef : desc.fields) {
                if (ef.name == f.name) {
                    ef = f; // update existing
                    found = true;
                    break;
                }
            }
            if (!found) desc.fields.push_back(f);
        }
        
        // Merge methods (append new ones)
        for (auto& m : methods) {
            bool found = false;
            for (auto& em : desc.methods) {
                if (em.name == m.name) {
                    em = m; // update existing
                    found = true;
                    break;
                }
            }
            if (!found) desc.methods.push_back(m);
        }
        
        desc.size = sizeof(T);
        // Store extractor for T* — extracts void* from std::any containing T*
        desc.extractPointer = [](const std::any& value) -> void* {
            try {
                // The any contains T* (pointer), cast to T* not T**
                return std::any_cast<std::add_pointer_t<T>>(value);
            } catch (...) {
                return nullptr;
            }
        };
        types_[t] = desc;
        types_[std::type_index(typeid(T*))] = desc; // pointer alias
        nameToType_[name] = t;
    }

    [[nodiscard]] bool isRegistered(const std::type_index& type) const {
        return types_.contains(type);
    }

    [[nodiscard]] bool isRegistered(const std::string& name) const {
        auto it = nameToType_.find(name);
        return it != nameToType_.end() && it->second.has_value();
    }

    [[nodiscard]] bool isRegistered(const std::optional<std::type_index>& type) const {
        return type.has_value() && types_.contains(type.value());
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
        if (it != nameToType_.end() && it->second.has_value()) {
            return getDescriptor(it->second.value());
        }
        return std::nullopt;
    }

    [[nodiscard]] const std::unordered_map<std::type_index, TypeDescriptor>& allTypes() const {
        return types_;
    }

private:
    std::unordered_map<std::type_index, TypeDescriptor> types_;
    std::unordered_map<std::string, std::optional<std::type_index>> nameToType_;
};

} // namespace fastrules
