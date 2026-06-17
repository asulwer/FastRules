/**
 * @file lua_backend.hpp
 * @brief Abstract Lua backend interface
 * 
 * LuaBackend defines the interface that LuaEngine uses to interact
 * with Lua. This abstraction allows different Lua implementations
 * to be plugged in (e.g., LuaBridge3, sol2, custom).
 * 
 * Current implementation uses LuaBridge3.
 * 
 * Design Philosophy:
 * - Minimal surface area for swapping backends
 * - Value-based interface (LuaValue) isolates engine from Lua details
 * - Supports compilation, evaluation, coroutines, and registration
 * 
 * Thread Safety:
 * Implementations are NOT required to be thread-safe. LuaEngine
 * handles synchronization at a higher level.
 * 
 * Memory Management:
 * LuaValue objects are owned by the caller. Backend implementations
 * should use reference counting or registry references for Lua objects.
 */

#pragma once

#include <memory>
#include <vector>
#include <string>
#include <any>
#include <functional>
#include <typeindex>

// Forward declaration
struct lua_State;

namespace fastrules {

// Forward declarations
class TypeRegistry;
class ActionCallbacks;

/**
 * @brief Lua value types
 * 
 * Enumeration of Lua value types exposed through the backend.
 */
enum class LuaType {
    Nil,        ///< Lua nil
    Boolean,    ///< true/false
    Number,     ///< Floating point number
    Integer,    ///< Integer number
    String,     ///< String
    Table,      ///< Lua table
    Function,   ///< Lua function
    Userdata    ///< Userdata (light or full)
};

/**
 * @brief Abstract wrapper for Lua values
 * 
 * LuaValue provides type-safe access to Lua values without exposing
 * the underlying Lua state. Implementations manage the lifetime of
 * the wrapped Lua object (usually via registry references).
 * 
 * Thread Safety: NOT thread-safe
 */
class LuaValue {
public:
    virtual ~LuaValue() = default;

    /// @brief Get the Lua type
    [[nodiscard]] virtual LuaType type() const = 0;

    /// @brief Check if nil
    [[nodiscard]] virtual bool isNil() const = 0;
    
    /// @brief Check if a string
    [[nodiscard]] virtual bool isString() const = 0;
    
    /// @brief Check if a table
    [[nodiscard]] virtual bool isTable() const = 0;

    /// @brief Convert to boolean
    [[nodiscard]] virtual bool toBool() const = 0;
    
    /// @brief Convert to double
    [[nodiscard]] virtual double toNumber() const = 0;
    
    /// @brief Convert to integer
    [[nodiscard]] virtual int64_t toInteger() const = 0;
    
    /// @brief Convert to string
    [[nodiscard]] virtual std::string toString() const = 0;

    /// @brief Get table length (number of elements)
    [[nodiscard]] virtual size_t length() const = 0;
    
    /// @brief Get table keys
    [[nodiscard]] virtual std::vector<std::string> keys() const = 0;
    
    /// @brief Get table field
    [[nodiscard]] virtual std::unique_ptr<LuaValue> get(const std::string& key) const = 0;
    
    /// @brief Set table field
    virtual void set(const std::string& key, const LuaValue& value) = 0;
};

/**
 * @brief Native function signature for registered functions
 * 
 * Functions registered with registerFunction() must match this signature.
 * 
 * @param L The Lua state (for advanced operations)
 * @param args Vector of argument values
 * @return Vector of return values
 */
using LuaNativeFunc = std::function<std::vector<std::unique_ptr<LuaValue>>(
    lua_State* L, const std::vector<std::unique_ptr<LuaValue>>& args)>;

/**
 * @brief Predicate function signature for registered predicates
 * 
 * Predicates must return a boolean result.
 * 
 * @param L The Lua state
 * @param args Vector of argument values
 * @return true if predicate passes, false otherwise
 */
using LuaPredicateFunc = std::function<bool(
    lua_State* L, const std::vector<std::unique_ptr<LuaValue>>& args)>;

/**
 * @brief Abstract Lua backend interface
 * 
 * Defines the contract that LuaEngine expects from a Lua backend.
 * All methods must be implemented by concrete backends.
 * 
 * Thread Safety: NOT required to be thread-safe
 */
class LuaBackend {
public:
    virtual ~LuaBackend() = default;

    /**
     * @brief Create a new backend instance
     * 
     * Factory method for creating backends.
     * 
     * @return Unique pointer to a new backend
     */
    [[nodiscard]] static std::unique_ptr<LuaBackend> create();

    // ========================================================================
    // Compilation
    // ========================================================================
    
    /**
     * @brief Compile an expression
     * 
     * Compiles the expression and stores it under the given ID.
     * The expression should evaluate to a value.
     * 
     * @param id Unique identifier for this compiled expression
     * @param code The Lua code to compile
     */
    virtual void compileExpression(const std::string& id, const std::string& code) = 0;

    /**
     * @brief Compile an action
     * 
     * Compiles the action code and stores it under the given ID.
     * Actions are statements, not expressions.
     * 
     * @param id Unique identifier for this compiled action
     * @param code The Lua code to compile
     */
    virtual void compileAction(const std::string& id, const std::string& code) = 0;

    // ========================================================================
    // Execution
    // ========================================================================
    
    /**
     * @brief Evaluate a compiled expression
     * 
     * @param id The compiled expression ID
     * @param params Parameters to set as globals before evaluation
     * @return The result value
     */
    [[nodiscard]] virtual std::unique_ptr<LuaValue> evaluate(
        const std::string& id,
        const std::vector<std::pair<std::string, std::any>>& params) = 0;

    /**
     * @brief Execute a compiled action
     * 
     * @param id The compiled action ID
     * @param params Parameters to set as globals before execution
     */
    virtual void executeAction(
        const std::string& id,
        const std::vector<std::pair<std::string, std::any>>& params) = 0;

    // ========================================================================
    // Coroutines
    // ========================================================================
    
    /**
     * @brief Create a coroutine from compiled code
     * 
     * @param id The compiled code ID
     * @return Opaque handle to the coroutine
     */
    [[nodiscard]] virtual void* createCoroutine(const std::string& id) = 0;

    /**
     * @brief Resume a coroutine
     * 
     * @param handle The coroutine handle
     * @return true if coroutine finished, false if yielded
     */
    [[nodiscard]] virtual bool resumeCoroutine(void* handle) = 0;

    /**
     * @brief Close a coroutine
     * 
     * Releases resources associated with the coroutine.
     * 
     * @param handle The coroutine handle
     */
    virtual void closeCoroutine(void* handle) = 0;

    // ========================================================================
    // Reference Lifecycle
    // ========================================================================
    
    /**
     * @brief Remove compiled code
     * 
     * Releases resources associated with the compiled code.
     * 
     * @param id The compiled code ID
     */
    virtual void removeCompiled(const std::string& id) = 0;

    // ========================================================================
    // Globals
    // ========================================================================
    
    /**
     * @brief Set a global variable
     * 
     * @param name The global name
     * @param value The value to set
     */
    virtual void setGlobal(const std::string& name, const LuaValue& value) = 0;

    /**
     * @brief Get a global variable
     * 
     * @param name The global name
     * @return The value, or nil if not found
     */
    [[nodiscard]] virtual std::unique_ptr<LuaValue> getGlobal(const std::string& name) = 0;

    /**
     * @brief Clear all custom globals
     * 
     * Removes globals that were set via setGlobal().
     */
    virtual void clearGlobals() = 0;

    // ========================================================================
    // Native Function / Predicate Registration
    // ========================================================================
    
    /**
     * @brief Register a native function
     * 
     * @param name The function name in Lua
     * @param func The C++ function to call
     */
    virtual void registerFunction(const std::string& name, LuaNativeFunc func) = 0;

    /**
     * @brief Register a predicate function
     * 
     * @param name The predicate name in Lua
     * @param func The C++ predicate to call
     */
    virtual void registerPredicate(const std::string& name, LuaPredicateFunc func) = 0;

    // ========================================================================
    // State Management
    // ========================================================================
    
    /**
     * @brief Open standard Lua libraries
     * 
     * Called during engine initialization.
     */
    virtual void openLibraries() = 0;

    /**
     * @brief Get the underlying Lua state
     * 
     * @return The lua_State pointer
     */
    [[nodiscard]] virtual lua_State* state() const = 0;

    /**
     * @brief Get a native state wrapper (for sol2 compatibility)
     * 
     * @return Native state pointer, or nullptr if not supported
     */
    [[nodiscard]] virtual void* nativeState() const = 0;

    /**
     * @brief Reset the Lua state
     * 
     * Closes and recreates the Lua state.
     */
    virtual void reset() = 0;

    /**
     * @brief Force garbage collection
     */
    virtual void collectGarbage() = 0;

    /**
     * @brief Get current memory usage
     * 
     * @return Memory used in kilobytes
     */
    [[nodiscard]] virtual size_t memoryUsageKB() const = 0;

    // ========================================================================
    // Value Creation Helpers
    // ========================================================================
    
    [[nodiscard]] virtual std::unique_ptr<LuaValue> makeNil() = 0;
    [[nodiscard]] virtual std::unique_ptr<LuaValue> makeBool(bool value) = 0;
    [[nodiscard]] virtual std::unique_ptr<LuaValue> makeInt(int value) = 0;
    [[nodiscard]] virtual std::unique_ptr<LuaValue> makeDouble(double value) = 0;
    [[nodiscard]] virtual std::unique_ptr<LuaValue> makeString(const std::string& value) = 0;
    [[nodiscard]] virtual std::unique_ptr<LuaValue> makeString(const char* value) = 0;
    [[nodiscard]] virtual std::unique_ptr<LuaValue> makePointer(void* ptr) = 0;

    /**
     * @brief Create a new empty table
     * 
     * @return A new table value
     */
    [[nodiscard]] virtual std::unique_ptr<LuaValue> createTable() = 0;

    // ========================================================================
    // Type / Action Binding
    // ========================================================================
    
    /**
     * @brief Bind registered types to Lua
     * 
     * @param registry The type registry
     */
    virtual void bindTypes(TypeRegistry* registry) = 0;

    /**
     * @brief Bind registered actions to Lua
     * 
     * @param callbacks The action callbacks
     */
    virtual void bindActions(ActionCallbacks* callbacks) = 0;

    /**
     * @brief Set a global for a registered type instance
     * 
     * @param name The variable name
     * @param type The C++ type
     * @param value The instance value
     * @param registry The type registry
     */
    virtual void setRegisteredTypeGlobal(const std::string& name,
                                         const std::type_index& type,
                                         const std::any& value,
                                         TypeRegistry* registry) = 0;

    /**
     * @brief Clear a registered type global
     * 
     * @param name The variable name
     */
    virtual void clearRegisteredTypeGlobal(const std::string& name) = 0;
};

} // namespace fastrules
