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
 - Fields are accessed via memory offset and memcpy
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

// Forward declaration for Lua C API
struct lua_State;

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
        TypeRegistrar& bind(const std::string& name, FieldType T::* ptr);

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
                               ReturnType (T::*ptr)(Args...));

    private:
        TypeDescriptor& desc_;  ///< The descriptor being populated
        
        /**
         * @brief Get Lua type string for C++ type
         * 
         * @tparam FieldType The C++ type
         * @return Lua type string
         */
        template<typename FieldType>
        std::string getLuaType();
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

} // namespace fastrules
