/**
 * @file type_registry.hpp
 * @brief C++ type registration for Lua binding
 * 
 * TypeRegistry allows C++ types to be used in Lua expressions.
 * This enables rules to reference C++ objects naturally:
 * @code
 * customer.age >= 18
 * @endcode
 * 
 * Registration Process:
 * 1. Define field offsets using offsetof()
 * 2. Register each field with its name and Lua type
 * 3. Optionally register methods
 * 4. Bind type to Lua state via engine.bindTypesToState()
 * 
 * Field Access:
 * - Fields are accessed via memory offset and memcpy
 * - Type safety is enforced at registration time
 * - Custom types need extractPointer function
 * 
 * Thread Safety:
 * - Registration: NOT thread-safe (do at startup)
 * - Lookup: Thread-safe (read-only after registration)
 * 
 * Example:
 * @code
 * struct Customer {
 *     std::string name;
 *     int age;
 *     bool isPremium() const { return age > 50; }
 * };
 * 
 * TypeRegistry registry;
 * registry.registerType<Customer>("Customer")
 *     .addField("name", offsetof(Customer, name), "string")
 *     .addField("age", offsetof(Customer, age), "int")
 *     .addMethod("isPremium", &Customer::isPremium);
 * 
 * // In engine:
 * engine.registerType<Customer>("Customer", [](auto& reg) {
 *     reg.bind("name", &Customer::name);
 *     reg.bind("age", &Customer::age);
 *     reg.method("isPremium", &Customer::isPremium);
 * });
 * @endcode
 */

#pragma once

#include <string>
#include <any>
#include <functional>
#include <optional>
#include <unordered_map>
#include <typeindex>
#include <type_traits>
#include <cstdint>
#include <tuple>
#include <utility>

// Lua C API is required for method invoker generation
#include <lua.hpp>

namespace fastrules {

/**
 * @brief Description of a type field
 * 
 * Contains metadata needed to access a C++ field from Lua:
 * - Field name in Lua
 * - Byte offset from object start
 * - Lua type string for conversion
 */
struct TypeField {
    std::string name;      ///< Field name in Lua
    size_t offset;         ///< Byte offset from object start
    std::string luaType;   ///< Lua type: "int", "double", "bool", "string"
};

/**
 * @brief Description of a type method
 * 
 * Methods are stored with an invoker function that handles the
 * C++ to Lua argument/return conversion.
 */
struct TypeMethod {
    std::string name;      ///< Method name in Lua
    
    /**
     * @brief Function to invoke the method
     * 
     * Signature: invoker(objectPtr, luaState) -> numberOfReturns
     */
    std::function<int(void*, lua_State*)> invoker;
};

/**
 * @brief Complete type descriptor
 * 
 * Contains all information needed to bind a C++ type to Lua:
 * - Type name in Lua
 * - List of fields with offsets
 * - List of methods
 * - Pointer extraction function
 */
struct TypeDescriptor {
    std::string name;                           ///< Type name in Lua
    std::vector<TypeField> fields;             ///< List of fields
    std::vector<TypeMethod> methods;            ///< List of methods
    
    /**
     * @brief Function to extract raw pointer from std::any
     * 
     * Needed because std::any_cast<void*> doesn't work directly.
     * The implementation must know the concrete type.
     */
    std::function<void*(const std::any&)> extractPointer;
};

/**
 * @brief Registry for C++ types
 * 
 * Manages type descriptors and provides lookup by std::type_index.
 * The registry itself is stateless - just a map of type descriptors.
 * 
 * Usage:
 * @code
 * TypeRegistry registry;
 * 
 * // Register type
 * registry.registerType<MyType>("MyType", [](auto& reg) {
 *     reg.bind("field1", &MyType::field1);
 *     reg.bind("field2", &MyType::field2);
 * });
 * 
 * // Check registration
 * if (registry.isRegistered(typeid(MyType))) {
 *     auto desc = registry.getDescriptor(typeid(MyType));
 * }
 * @endcode
 */
class TypeRegistry {
public:
    /// @brief Default constructor
    TypeRegistry() = default;
    
    /// @brief Default destructor
    ~TypeRegistry() = default;
    
    /// @brief Copy constructor
    TypeRegistry(const TypeRegistry&) = default;
    
    /// @brief Copy assignment
    TypeRegistry& operator=(const TypeRegistry&) = default;
    
    /// @brief Move constructor
    TypeRegistry(TypeRegistry&&) = default;
    
    /// @brief Move assignment
    TypeRegistry& operator=(TypeRegistry&&) = default;

    /**
     * @brief Register a C++ type
     * 
     * Template method that captures the type and creates a registrar.
     * The registrar callback configures fields and methods.
     * 
     * @tparam T The C++ type to register
     * @tparam Registrar Callable type for configuration
     * @param name The Lua name for this type
     * @param registrar Callback to configure bindings
     * 
     * Example:
     * @code
     * registry.registerType<Customer>("Customer", [](auto& reg) {
     *     reg.bind("name", &Customer::name);
     *     reg.bind("age", &Customer::age);
     * });
     * @endcode
     */
    template<typename T, typename Registrar>
    void registerType(const std::string& name, Registrar registrar);

    /**
     * @brief Check if a type is registered
     * 
     * @param type The std::type_index to check
     * @return true if registered, false otherwise
     */
    [[nodiscard]] bool isRegistered(const std::type_index& type) const;

    /**
     * @brief Get descriptor for a registered type
     * 
     * @param type The std::type_index
     * @return The TypeDescriptor, or nullopt if not found
     */
    [[nodiscard]] std::optional<TypeDescriptor> getDescriptor(
        const std::type_index& type) const;

    /**
     * @brief Get all registered types
     * 
     * @return Const reference to the type map
     */
    [[nodiscard]] const std::unordered_map<std::type_index, TypeDescriptor>& 
        allTypes() const { return types_; }

    /**
     * @brief Registrar helper for type binding
     * 
     * Provides a fluent API for binding fields and methods.
     * Created by registerType and passed to the registrar callback.
     * 
     * @tparam T The C++ type being registered
     */
    template<typename T>
    class TypeRegistrar {
    public:
        /**
         * @brief Construct with descriptor reference
         * 
         * @param desc The TypeDescriptor to populate
         */
        explicit TypeRegistrar(TypeDescriptor& desc) : desc_(desc) {}

        /**
         * @brief Bind a field
         * 
         * @tparam FieldType The field type
         * @param name Lua field name
         * @param ptr Member pointer to field
         * @return Reference for chaining
         * 
         * Example:
         * @code
         * reg.bind("age", &Customer::age);
         * @endcode
         */
        template<typename FieldType>
        TypeRegistrar& bind(const std::string& name, FieldType T::* ptr) {
            TypeField field;
            field.name = name;
            // Common offset-from-member-pointer idiom. Cast through uintptr_t
            // to avoid compiler warnings about dereferencing null.
            field.offset = static_cast<size_t>(
                reinterpret_cast<uintptr_t>(
                    &((reinterpret_cast<T*>(0)->*ptr))));
            field.luaType = getLuaType<FieldType>();
            desc_.fields.push_back(std::move(field));
            return *this;
        }

        /**
         * @brief Bind a method
         * 
         * @tparam ReturnType The return type
         * @tparam Args The argument types
         * @param name Lua method name
         * @param ptr Member pointer to method
         * @return Reference for chaining
         * 
         * Example:
         * @code
         * reg.method("isPremium", &Customer::isPremium);
         * @endcode
         */
        template<typename ReturnType, typename... Args>
        TypeRegistrar& method(const std::string& name,
                              ReturnType (T::*ptr)(Args...)) {
            addMethod(name, ptr);
            return *this;
        }

        template<typename ReturnType, typename... Args>
        TypeRegistrar& method(const std::string& name,
                              ReturnType (T::*ptr)(Args...) const) {
            addMethod(name, ptr);
            return *this;
        }

    private:
        TypeDescriptor& desc_;  ///< The descriptor being populated
        
        /**
         * @brief Get Lua type string for C++ type
         * 
         * @tparam FieldType The C++ type
         * @return Lua type string
         */
        template<typename FieldType>
        std::string getLuaType() {
            std::string result = "unknown";
            if constexpr (std::is_same_v<FieldType, bool>) {
                result = "bool";
            } else if constexpr (std::is_same_v<FieldType, int> ||
                                 std::is_same_v<FieldType, unsigned int> ||
                                 std::is_same_v<FieldType, long> ||
                                 std::is_same_v<FieldType, unsigned long> ||
                                 std::is_same_v<FieldType, short> ||
                                 std::is_same_v<FieldType, unsigned short> ||
                                 std::is_same_v<FieldType, long long> ||
                                 std::is_same_v<FieldType, unsigned long long> ||
                                 std::is_same_v<FieldType, char>) {
                result = "int";
            } else if constexpr (std::is_same_v<FieldType, double> ||
                                 std::is_same_v<FieldType, float>) {
                result = "double";
            } else if constexpr (std::is_same_v<FieldType, std::string>) {
                result = "string";
            }
            return result;
        }

        template<typename ReturnType, typename... Args>
        void addMethod(const std::string& name,
                       ReturnType (T::*ptr)(Args...)) {
            TypeMethod m;
            m.name = name;
            m.invoker = buildInvoker(ptr);
            desc_.methods.push_back(std::move(m));
        }

        template<typename ReturnType, typename... Args>
        void addMethod(const std::string& name,
                       ReturnType (T::*ptr)(Args...) const) {
            TypeMethod m;
            m.name = name;
            m.invoker = buildInvoker(ptr);
            desc_.methods.push_back(std::move(m));
        }

        // Helpers to read Lua arguments into a tuple
        template<typename Arg>
        static Arg readArg(lua_State* L, int index) {
            if constexpr (std::is_same_v<Arg, int>) {
                return static_cast<int>(lua_tointeger(L, index));
            } else if constexpr (std::is_same_v<Arg, double>) {
                return lua_tonumber(L, index);
            } else if constexpr (std::is_same_v<Arg, float>) {
                return static_cast<float>(lua_tonumber(L, index));
            } else if constexpr (std::is_same_v<Arg, bool>) {
                return lua_toboolean(L, index) != 0;
            } else if constexpr (std::is_same_v<Arg, std::string>) {
                const char* s = lua_tostring(L, index);
                return s ? std::string(s) : std::string();
            } else {
                static_assert(std::is_same_v<Arg, void>,
                              "Unsupported method argument type");
                return Arg{};
            }
        }

        template<typename... Args, size_t... I>
        static std::tuple<Args...> readArgs(lua_State* L, int base,
                                            std::index_sequence<I...>) {
            return std::make_tuple(readArg<Args>(L, base + static_cast<int>(I))...);
        }

        template<typename ReturnType, typename... Args, size_t... I>
        static ReturnType callMethod(T* self,
                                     ReturnType (T::*ptr)(Args...),
                                     const std::tuple<Args...>& args,
                                     std::index_sequence<I...>) {
            return (self->*ptr)(std::get<I>(args)...);
        }

        template<typename ReturnType, typename... Args, size_t... I>
        static ReturnType callMethod(T* self,
                                     ReturnType (T::*ptr)(Args...) const,
                                     const std::tuple<Args...>& args,
                                     std::index_sequence<I...>) {
            return (self->*ptr)(std::get<I>(args)...);
        }

        template<typename ReturnType>
        static void pushReturn(lua_State* L, ReturnType&& value) {
            using Decayed = std::decay_t<ReturnType>;
            if constexpr (std::is_same_v<Decayed, bool>) {
                lua_pushboolean(L, value);
            } else if constexpr (std::is_integral_v<Decayed>) {
                lua_pushinteger(L, static_cast<lua_Integer>(value));
            } else if constexpr (std::is_floating_point_v<Decayed>) {
                lua_pushnumber(L, static_cast<lua_Number>(value));
            } else if constexpr (std::is_same_v<Decayed, std::string>) {
                lua_pushstring(L, value.c_str());
            } else if constexpr (std::is_same_v<Decayed, const char*>) {
                lua_pushstring(L, value);
            } else if constexpr (!std::is_void_v<Decayed>) {
                lua_pushnil(L);
            }
        }

        template<typename ReturnType, typename... Args>
        std::function<int(void*, lua_State*)> buildInvoker(ReturnType (T::*ptr)(Args...)) {
            return [ptr](void* obj, lua_State* L) -> int {
                T* self = static_cast<T*>(obj);
                constexpr size_t arity = sizeof...(Args);
                auto args = readArgs<Args...>(L, 2, std::make_index_sequence<arity>{});
                if constexpr (std::is_void_v<ReturnType>) {
                    callMethod(self, ptr, args, std::make_index_sequence<arity>{});
                    return 0;
                } else {
                    ReturnType result = callMethod(self, ptr, args,
                                                   std::make_index_sequence<arity>{});
                    pushReturn(L, std::move(result));
                    return 1;
                }
            };
        }

        template<typename ReturnType, typename... Args>
        std::function<int(void*, lua_State*)> buildInvoker(ReturnType (T::*ptr)(Args...) const) {
            return [ptr](void* obj, lua_State* L) -> int {
                T* self = static_cast<T*>(obj);
                constexpr size_t arity = sizeof...(Args);
                auto args = readArgs<Args...>(L, 2, std::make_index_sequence<arity>{});
                if constexpr (std::is_void_v<ReturnType>) {
                    callMethod(self, ptr, args, std::make_index_sequence<arity>{});
                    return 0;
                } else {
                    ReturnType result = callMethod(self, ptr, args,
                                                   std::make_index_sequence<arity>{});
                    pushReturn(L, std::move(result));
                    return 1;
                }
            };
        }
    };

private:
    /// @brief Map of type_index to descriptors
    std::unordered_map<std::type_index, TypeDescriptor> types_;
};

// Inline implementations
inline bool TypeRegistry::isRegistered(const std::type_index& type) const {
    return types_.find(type) != types_.end();
}

inline std::optional<TypeDescriptor> TypeRegistry::getDescriptor(
    const std::type_index& type) const {
    auto it = types_.find(type);
    if (it != types_.end()) {
        return it->second;
    }
    return std::nullopt;
}

// Template implementation for registerType
template<typename T, typename Registrar>
void TypeRegistry::registerType(const std::string& name, Registrar registrar) {
    std::type_index key(typeid(T));

    // If the type is already registered, merge the new bindings into the
    // existing descriptor. This allows macros like FASTRULES_REGISTER_METHODS_N
    // to add methods after FASTRULES_REGISTER_TYPE_N has already bound fields.
    TypeDescriptor descriptor;
    auto it = types_.find(key);
    if (it != types_.end()) {
        descriptor = it->second;
    }

    // Ensure the name and pointer extractor are always present.
    descriptor.name = name;
    descriptor.extractPointer = [](const std::any& value) -> void* {
        try {
            return std::any_cast<T*>(value);
        } catch (...) {
            return nullptr;
        }
    };

    TypeRegistrar<T> reg(descriptor);
    registrar(reg);

    types_[key] = std::move(descriptor);
}

} // namespace fastrules
