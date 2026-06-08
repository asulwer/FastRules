#pragma once

#ifdef FASTRULES_USE_SOL2
#include <sol/sol.hpp>
#endif
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <any>
#include <typeindex>
#include <typeinfo>
#include <memory>
#include <optional>

namespace fastrules {

// ============================================================================
// Type Registry - Maps C++ types to Lua usertypes
// ============================================================================

#ifdef FASTRULES_USE_SOL2

// Abstract base for type binders
class TypeBinderBase {
public:
    virtual ~TypeBinderBase() = default;
    virtual void bind(sol::state& lua, const std::string& name) = 0;
    virtual sol::object toLua(sol::state& lua, std::any& value) = 0;
    virtual sol::object toLua(sol::state& lua, const std::any& value) const = 0;
};

// Concrete type binder using sol2 usertype
template<typename T>
class TypeBinder : public TypeBinderBase {
public:
    using BinderFunc = std::function<void(sol::usertype<T>&)>;

    explicit TypeBinder(BinderFunc binder) : binder_(std::move(binder)) {}

    void bind(sol::state& lua, const std::string& name) override {
        auto ut = lua.new_usertype<T>(name, sol::no_constructor);
        binder_(ut);
    }

    sol::object toLua(sol::state& lua, std::any& value) override {
        try {
            if (auto ptr = std::any_cast<T*>(&value); ptr) {
                return sol::make_object(lua, *ptr);
            }
            auto& ref = std::any_cast<T&>(value);
            return sol::make_object(lua, &ref);
        } catch (...) {
            return sol::nil;
        }
    }

    sol::object toLua(sol::state& lua, const std::any& value) const override {
        try {
            if (auto ptr = std::any_cast<T*>(&value); ptr) {
                return sol::make_object(lua, *ptr);
            }
            auto& ref = std::any_cast<const T&>(value);
            return sol::make_object(lua, const_cast<T*>(&ref));
        } catch (...) {
            return sol::nil;
        }
    }

private:
    BinderFunc binder_;
};

// Central registry for all user-defined types
class TypeRegistry {
public:
    TypeRegistry() = default;

    template<typename T>
    void registerType(const std::string& name, typename TypeBinder<T>::BinderFunc binder) {
        auto binderPtr = std::make_shared<TypeBinder<T>>(std::move(binder));
        types_[std::type_index(typeid(T))] = {name, binderPtr};
        types_[std::type_index(typeid(T*))] = {name, binderPtr};
        nameToType_[name] = std::optional<std::type_index>(std::type_index(typeid(T)));
    }

    [[nodiscard]] bool isRegistered(const std::type_index& type) const {
        return types_.contains(type);
    }

    [[nodiscard]] bool isRegistered(const std::string& name) const {
        auto it = nameToType_.find(name); return it != nameToType_.end() && it->second.has_value();
    }

    [[nodiscard]] std::optional<std::string> getTypeName(const std::type_index& type) const {
        auto it = types_.find(type);
        if (it != types_.end()) {
            return it->second.name;
        }
        return std::nullopt;
    }

    void bindAll(sol::state& lua) const {
        std::unordered_set<std::string> boundNames;
        for (const auto& [typeIndex, entry] : types_) {
            if (boundNames.insert(entry.name).second) {
                entry.binder->bind(lua, entry.name);
            }
        }
    }

    [[nodiscard]] sol::object toLua(sol::state& lua, std::any& value) const {
        if (!value.has_value()) return sol::nil;
        const auto& ti = value.type();
        auto it = types_.find(std::type_index(ti));
        if (it != types_.end()) {
            return it->second.binder->toLua(lua, value);
        }
        return sol::nil;
    }

    [[nodiscard]] sol::object toLua(sol::state& lua, const std::any& value) const {
        if (!value.has_value()) return sol::nil;
        const auto& ti = value.type();
        auto it = types_.find(std::type_index(ti));
        if (it != types_.end()) {
            return it->second.binder->toLua(lua, value);
        }
        return sol::nil;
    }

private:
    struct TypeIndexHash {
        std::size_t operator()(const std::type_index& t) const {
            return t.hash_code();
        }
    };

    struct RegistryEntry {
        std::string name;
        std::shared_ptr<TypeBinderBase> binder;
    };

    std::unordered_map<std::type_index, RegistryEntry, TypeIndexHash> types_;
    std::unordered_map<std::string, std::optional<std::type_index>> nameToType_;
};

#else  // !FASTRULES_USE_SOL2

// Stub TypeRegistry for non-sol2 backends (e.g., LuaBridge3)
class TypeRegistry {
public:
    TypeRegistry() = default;

    template<typename T>
    void registerType(const std::string&, auto) {}

    [[nodiscard]] bool isRegistered(const std::type_index&) const { return false; }
    [[nodiscard]] bool isRegistered(const std::string&) const { return false; }
    [[nodiscard]] std::optional<std::string> getTypeName(const std::type_index&) const { return std::nullopt; }
    void bindAll(auto&) const {}
    [[nodiscard]] auto toLua(auto&, std::any&) const { return nullptr; }
    [[nodiscard]] auto toLua(auto&, const std::any&) const { return nullptr; }
};

#endif // FASTRULES_USE_SOL2

// Convenience macro for registering a simple struct
#ifdef FASTRULES_USE_SOL2
#define REGISTER_TYPE(registry, name, T, ...) \
    registry.registerType<T>(name, [](sol::usertype<T>& ut) { \
        ut[#__VA_ARGS__] = &T::__VA_ARGS__; \
    })
#else
#define REGISTER_TYPE(registry, name, T, ...) (void)0
#endif

} // namespace fastrules
