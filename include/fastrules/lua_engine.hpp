/**
 * @file lua_engine.hpp
 * @brief Main Lua scripting engine - compiles and executes Lua expressions
 * 
 * LuaEngine is the core scripting component of FastRules. It provides:
 * - Lua expression compilation (to bytecode references)
 * - Lua expression evaluation with parameters
 * - Action execution
 * - Coroutine support for async operations
 * - Type registration for C++ types
 * - Built-in predicate functions
 * - Memory management and garbage collection
 * - Execution timeout enforcement
 * 
 * Architecture:
 * LuaEngine wraps a LuaBackend (currently LuaBridge3Backend) which abstracts
 * the underlying Lua implementation. This allows future backends (e.g., sol2)
 * without changing the public API.
 * 
 * Thread Safety:
 * - Construction: Thread-safe
 * - Compilation: Thread-safe (uses mutex internally)
 * - Evaluation: NOT thread-safe (single Lua state)
 * 
 * For parallel execution, use clone() to create thread-local engine copies,
 * or use Workflow::executeParallel() which manages the pool automatically.
 * 
 * Memory Management:
 * LuaEngine automatically tracks compiled references and provides resetState()
 * for cleaning up. The autoResetThresholdKB_ setting can trigger automatic
 * resets when memory usage exceeds a threshold.
 * 
 * Timeout Enforcement:
 * Timeouts are enforced using Lua debug hooks that check a thread-local
 * deadline every 1000 Lua instructions. This is cooperative - tight
 * native loops may exceed the timeout.
 * 
 * Security:
 * - Dangerous Lua functions are removed from the global environment
 * - Expressions can be validated before compilation
 * - Expression length limits prevent DoS
 * 
 * Example:
 * @code
 * fastrules::LuaEngine engine;
 * 
 * // Compile an expression
 * auto ref = engine.compileExpression("age >= 18");
 * 
 * // Evaluate with parameters
 * fastrules::RuleContext ctx;
 * std::vector<fastrules::RuleParameter> params;
 * params.emplace_back("age", 25);
 * 
 * bool result = engine.evaluateExpression(ref.value(), params, ctx);
 * 
 * // Cleanup
 * engine.releaseRef(ref.value());
 * @endcode
 */

#pragma once

#include "fastrules/lua_backend.hpp"
#include "fastrules/type_registry.hpp"
#include "fastrules/action_callback.hpp"
#include "fastrules/logger.hpp"

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <chrono>
#include <any>
#include <mutex>
#include <shared_mutex>

// Forward declaration for Lua C API
struct lua_State;

namespace fastrules {

// Forward declarations
class RuleContext;
struct RuleParameter;

/**
 * @brief Main Lua scripting engine for rule evaluation
 * 
 * LuaEngine wraps a Lua state and provides a high-level API for:
 * - Compiling expressions to bytecode references
 * - Evaluating expressions with C++ parameter binding
 * - Executing actions
 * - Managing coroutines
 * - Registering C++ types and functions
 * 
 * The engine maintains internal registries of compiled expressions
 * and coroutines, identified by integer references.
 * 
 * Performance Notes:
 * - Compilation is expensive - do it once and reuse references
 * - Evaluation is fast - just Lua bytecode execution
 * - Cloning creates a new Lua state - also expensive
 * - Prefer Workflow's engine pool over manual cloning
 */
class LuaEngine {
public:
    // ========================================================================
    // Construction / Destruction
    // ========================================================================
    
    /**
     * @brief Construct a new LuaEngine
     * 
     * Creates a new Lua state, opens standard libraries (with dangerous
     * functions removed), sets up the environment, and registers
     * built-in predicates.
     * 
     * This is relatively expensive - prefer reusing engines.
     */
    LuaEngine();
    
    /// @brief Destructor - closes coroutine handles and cleans up
    ~LuaEngine();

    /// @brief Move constructor
    LuaEngine(LuaEngine&& other) noexcept;
    
    /// @brief Move assignment
    LuaEngine& operator=(LuaEngine&& other) noexcept;
    
    /// @brief Disable copy (Lua state is unique)
    LuaEngine(const LuaEngine&) = delete;
    
    /// @brief Disable copy assignment
    LuaEngine& operator=(const LuaEngine&) = delete;

    /**
     * @brief Create a shallow copy of this engine
     * 
     * Clones the engine's type registry and action callbacks but NOT
     * the compiled references. The cloned engine will need to recompile
     * expressions, but registered types and actions are preserved.
     * 
     * Used by Workflow for parallel execution.
     * 
     * @return A new LuaEngine with copied configuration
     */
    [[nodiscard]] std::unique_ptr<LuaEngine> clone() const;

    // ========================================================================
    // Compilation
    // ========================================================================
    
    /**
     * @brief Compile a Lua expression
     * 
     * Compiles the expression to Lua bytecode and stores it in the
     * engine's registry. Returns an integer reference that can be
     * used for evaluation.
     * 
     * The expression should evaluate to a boolean or truthy/falsy value.
     * Parameters are automatically extracted from the expression text.
     * 
     * @param expression Lua expression string
     * @return Integer reference, or nullopt if expression is empty
     * @throws RuleCompilationException if compilation fails
     * 
     * Example:
     * @code
     * auto ref = engine.compileExpression("age >= 18");
     * if (ref.has_value()) {
     *     // Use ref for evaluation...
     * }
     * @endcode
     */
    [[nodiscard]] std::optional<int> compileExpression(const std::string& expression);

    /**
     * @brief Compile a Lua action
     * 
     * Similar to compileExpression but for actions (statements).
     * Actions don't need to return a value.
     * 
     * @param action Lua action code
     * @return Integer reference, or nullopt if action is empty
     * @throws RuleCompilationException if compilation fails
     */
    [[nodiscard]] std::optional<int> compileAction(const std::string& action);

    /**
     * @brief Compile a Lua coroutine
     * 
     * Compiles an expression as a Lua coroutine (thread).
     * Coroutines can be resumed multiple times and support
     * cooperative multitasking.
     * 
     * @param expression Lua expression string
     * @return Integer reference to coroutine
     * @throws RuleCompilationException if compilation fails
     */
    [[nodiscard]] std::optional<int> compileCoroutine(const std::string& expression);

    /**
     * @brief Release a compiled reference
     * 
     * Removes the compiled code from the registry and frees memory.
     * Call this when you're done with a reference.
     * 
     * @param ref The reference to release
     */
    void releaseRef(int ref);

    /**
     * @brief Check if a reference is a coroutine
     * 
     * @param ref The reference to check
     * @return true if the reference was compiled as a coroutine
     */
    [[nodiscard]] bool isCoroutine(int ref) const;

    // ========================================================================
    // Evaluation
    // ========================================================================
    
    /**
     * @brief Evaluate a compiled expression
     * 
     * Evaluates the expression with the given parameters bound as
     * Lua globals. The context provides access to previous results.
     * 
     * @param ref The compiled expression reference
     * @param parameters Parameters to bind as Lua globals
     * @param context Execution context for result access
     * @param timeout Optional timeout (uses debug hooks)
     * @return true if expression evaluates to truthy, false otherwise
     * @throws RuleExecutionException if evaluation fails
     * @throws RuleTimeoutException if timeout is exceeded
     * 
     * Thread Safety: NOT thread-safe. Call from one thread at a time
     * or use separate LuaEngine instances per thread.
     */
    [[nodiscard]] bool evaluateExpression(int ref,
                                          const std::vector<RuleParameter>& parameters,
                                          RuleContext& context,
                                          std::optional<std::chrono::milliseconds> timeout = std::nullopt);

    /**
     * @brief Execute a compiled action
     * 
     * Executes the action code with parameters bound as Lua globals.
     * Actions don't return values but can modify state and call callbacks.
     * 
     * @param ref The compiled action reference
     * @param parameters Parameters to bind as Lua globals
     * @param context Execution context
     * @param timeout Optional timeout
     * @throws RuleExecutionException if execution fails
     * @throws RuleTimeoutException if timeout is exceeded
     */
    void executeAction(int ref,
                       const std::vector<RuleParameter>& parameters,
                       RuleContext& context,
                       std::optional<std::chrono::milliseconds> timeout = std::nullopt);

    // ========================================================================
    // Coroutines
    // ========================================================================
    
    /**
     * @brief Resume a coroutine
     * 
     * Resumes execution of a compiled coroutine. Returns true when
     * the coroutine completes, false if it yields.
     * 
     * @param ref The coroutine reference
     * @param parameters Parameters to bind
     * @param context Execution context
     * @return true if coroutine finished, false if yielded
     * @throws RuleExecutionException on error
     */
    [[nodiscard]] bool resumeCoroutine(int ref,
                                       const std::vector<RuleParameter>& parameters,
                                       RuleContext& context);

    /**
     * @brief Await a coroutine's completion
     * 
     * Repeatedly resumes the coroutine until it completes.
     * 
     * @param ref The coroutine reference
     * @param parameters Parameters to bind
     * @param context Execution context
     * @return Optional return value (currently always nullopt)
     */
    [[nodiscard]] std::optional<std::any> await(int ref,
                                                const std::vector<RuleParameter>& parameters,
                                                RuleContext& context);

    // ========================================================================
    // Type Registration
    // ========================================================================
    
    /**
     * @brief Register a C++ type with Lua
     * 
     * Binds a C++ type so it can be used in Lua expressions.
     * The registrar callback configures field and method bindings.
     * 
     * @tparam T The C++ type to register
     * @param name The Lua name for the type
     * @param registrar Callback to configure bindings
     * 
     * Example:
     * @code
     * engine.registerType<Customer>("Customer", [](auto& reg) {
     *     reg.bind("name", &Customer::name);
     *     reg.bind("age", &Customer::age);
     *     reg.method("isPremium", &Customer::isPremium);
     * });
     * @endcode
     */
    template<typename T, typename Registrar>
    void registerType(const std::string& name, Registrar registrar);

    /**
     * @brief Bind registered types to the Lua state
     * 
     * Called internally by compile() to ensure types are available.
     * Usually not called directly.
     */
    void bindTypesToState();

    /**
     * @brief Bind registered actions to the Lua state
     * 
     * Called internally by compile() to ensure actions are available.
     * Usually not called directly.
     */
    void bindActionsToState();

    /**
     * @brief Register an action callback
     * 
     * Registers a C++ function that can be called from Lua action code.
     * The action receives parameters as std::any values.
     * 
     * @param name The action name
     * @param handler The callback function
     * 
     * Example:
     * @code
     * engine.registerAction("sendEmail", [](const std::any& target, 
     *                                       const std::vector<std::any>& args) {
     *     // Handle the action
     * });
     * @endcode
     */
    void registerAction(const std::string& name, 
                       std::function<void(const std::any&, const std::vector<std::any>&)> handler);

    // ========================================================================
    // Callback Discovery
    // ========================================================================
    
    /**
     * @brief Auto-discover callback handlers from action code
     * 
     * Scans action code for "callbacks.XXX" patterns and registers
     * stub handlers for any unknown callbacks. This ensures actions
     * referencing callbacks work even if the handler wasn't explicitly
     * registered.
     * 
     * @param actions Vector of action code strings to scan
     */
    void discoverCallbacks(const std::vector<std::string>& actions);

    // ========================================================================
    // Globals
    // ========================================================================
    
    /**
     * @brief Set a global variable in Lua
     * 
     * Sets a Lua global variable to the given C++ value.
     * 
     * @param name The global variable name
     * @param value The C++ value (converted to Lua)
     */
    void setGlobal(const std::string& name, const std::any& value);

    /**
     * @brief Clear all globals set by this engine
     * 
     * Removes all globals that were set via setGlobal().
     * Called automatically after evaluation.
     */
    void clearGlobals();

    // ========================================================================
    // Accessors
    // ========================================================================
    
    /**
     * @brief Get the underlying Lua state
     * 
     * Provides access to the raw lua_State for advanced operations.
     * Use with caution - improper use can corrupt the engine state.
     * 
     * @return The lua_State pointer
     */
    [[nodiscard]] lua_State* luaState() const noexcept;

    /// @brief Get the type registry
    [[nodiscard]] TypeRegistry& typeRegistry() noexcept { return typeRegistry_; }
    
    /// @brief Get the type registry (const)
    [[nodiscard]] const TypeRegistry& typeRegistry() const noexcept { return typeRegistry_; }

    // ========================================================================
    // Configuration
    // ========================================================================
    
    /**
     * @brief Set maximum expression/action length
     * 
     * Limits the size of expressions that can be compiled.
     * 0 means no limit. Prevents DoS via huge expressions.
     * 
     * @param length Maximum length in bytes (0 = unlimited)
     */
    void setMaxExpressionLength(size_t length) { maxExpressionLength_ = length; }

    /**
     * @brief Set auto-reset memory threshold
     * 
     * When memory usage exceeds this threshold (in KB), the Lua state
     * is automatically reset. 0 disables auto-reset.
     * 
     * @param kb Threshold in kilobytes (0 = disabled)
     */
    void setAutoResetThreshold(size_t kb) { autoResetThresholdKB_ = kb; }

    /// @brief Get the number of successful compilations
    [[nodiscard]] int getCompileCount() const noexcept { return compileCount_.load(); }

    /// @brief Get the current state generation (incremented on reset)
    [[nodiscard]] int getGeneration() const noexcept { return generation_.load(); }

    // ========================================================================
    // State Management
    // ========================================================================
    
    /**
     * @brief Reset the Lua state
     * 
     * Closes the current Lua state and creates a new one.
     * Clears all compiled references and re-registers types/actions.
     * 
     * Use this to recover from memory leaks or Lua state corruption.
     * After reset, rules must be recompiled.
     */
    void resetState();

    /**
     * @brief Force garbage collection
     * 
     * Runs two full GC cycles to reclaim memory.
     */
    void collectGarbage();

    /**
     * @brief Get current memory usage
     * 
     * @return Memory used by Lua in kilobytes
     */
    [[nodiscard]] size_t getMemoryUsageKB();

    // ========================================================================
    // Logging
    // ========================================================================
    
    /**
     * @brief Set the logger for this engine
     * 
     * The logger is used for debug output, warnings, and errors.
     * If not set, logging is silently discarded.
     * 
     * @param logger Shared pointer to spdlog logger
     */
    void setLogger(std::shared_ptr<spdlog::logger> logger) { logger_ = logger; }
    
    /// @brief Get the configured logger
    [[nodiscard]] std::shared_ptr<spdlog::logger> getLogger() const noexcept { return logger_; }
    
    /// @brief Check if a logger is configured
    [[nodiscard]] bool hasLogger() const noexcept { return logger_ != nullptr; }

private:
    // ========================================================================
    // Internal Types
    // ========================================================================
    
    /// @brief Map of reference IDs to backend IDs
    std::unordered_map<int, std::string> refToBackendId_;
    
    /// @brief Set of references that are coroutines
    std::unordered_map<int, bool> coroutineRegistry_;
    
    /// @brief Map of coroutine handles
    std::unordered_map<int, void*> coroutineHandles_;
    
    /// @brief Map of reference IDs to parameter names
    std::unordered_map<int, std::vector<std::string>> paramNames_;
    
    /// @brief Next reference ID to assign
    int nextRefId_ = 1;

    // ========================================================================
    // Components
    // ========================================================================
    
    std::unique_ptr<LuaBackend> backend_;  ///< The Lua backend implementation
    TypeRegistry typeRegistry_;              ///< Registered C++ types
    ActionCallbacks actionCallbacks_;        ///< Registered action callbacks

    // ========================================================================
    // Configuration
    // ========================================================================
    
    size_t maxExpressionLength_ = 0;           ///< Max expression length (0 = unlimited)
    size_t autoResetThresholdKB_ = 0;        ///< Auto-reset threshold (0 = disabled)
    std::atomic<int> compileCount_{0};       ///< Number of successful compilations
    std::atomic<int> generation_{0};         ///< State generation (increments on reset)
    std::shared_ptr<spdlog::logger> logger_;           ///< spdlog logger

    // ========================================================================
    // Thread Safety
    // ========================================================================
    
    mutable std::mutex luaStateMutex_;                    ///< Protects Lua state operations
    mutable std::shared_mutex registryMutex_;             ///< Protects reference registries

    // ========================================================================
    // Thread-Local Context
    // ========================================================================
    
    static thread_local RuleContext* currentContext_;      ///< Current execution context

    // ========================================================================
    // Internal Methods
    // ========================================================================
    
    /**
     * @brief Setup the Lua environment
     * 
     * Removes dangerous globals, creates context table, and sets up
     * the timeout hook.
     */
    void setupEnvironment();

    /**
     * @brief Setup the context table for execution
     * 
     * Registers context_getResult as a Lua function.
     * 
     * @param context The execution context
     */
    void setupContextTable(RuleContext& context);

    /**
     * @brief Register built-in predicate functions
     * 
     * Registers: isNotNull, isNull, isEmpty, inRange, matchesRegex,
     * startsWith, endsWith, hasLength, hasMinLength, hasMaxLength,
     * countEquals, countGreaterThan, countLessThan, contains
     */
    void registerPredicates();

    /**
     * @brief Build parameter pairs for evaluation
     * 
     * Matches extracted parameter names with provided values.
     * 
     * @param ref The expression reference
     * @param parameters The provided parameters
     * @return Vector of (name, value) pairs
     */
    std::vector<std::pair<std::string, std::any>> buildParamPairs(
        int ref, const std::vector<RuleParameter>& parameters);

    /**
     * @brief Convert a LuaValue to std::any
     * 
     * @param value The LuaValue
     * @return std::any containing the value
     */
    [[nodiscard]] std::any luaValueToAny(const LuaValue& value) const;

    /**
     * @brief Create a backend ID from a reference
     * 
     * @param ref The reference ID
     * @return Backend ID string
     */
    [[nodiscard]] std::string makeBackendId(int ref) const;

    // Wrap expression/action for return (no-op with new backend)
    [[nodiscard]] std::string wrapExpression(const std::string& expression);
    [[nodiscard]] std::string wrapAction(const std::string& action);
    [[nodiscard]] std::string wrapCoroutine(const std::string& expression);
};

} // namespace fastrules
