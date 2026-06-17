/**
 * @file action_callback.hpp
 * @brief Action callback registration system
 * 
 * ActionCallbacks allows C++ functions to be called from Lua actions.
 * When a rule's action executes, it can call registered callbacks
 * to perform operations like sending emails, logging, or database updates.
 * 
 * Registration:
 * Callbacks are registered by name. The name becomes available in Lua
 * as "callbacks.name()".
 * 
 * Invocation:
 * From Lua actions: callbacks.sendEmail("user@example.com", "Hello")
 * Arguments are automatically converted from Lua to C++ types.
 * 
 * Thread Safety:
 * - Registration: NOT thread-safe (do at startup)
 * - Invocation: Thread-safe (read-only after registration)
 * 
 * Discovery:
 * The discoverCallbacks feature scans action code for "callbacks.X"
 * patterns and registers stub handlers for unknown callbacks.
 * 
 * Example:
 * @code
 * ActionCallbacks callbacks;
 * 
 * // Register a callback
 * callbacks.registerHandler("sendEmail", [](const std::any& target,
 *                                           const std::vector<std::any>& args) {
 *     std::string email = std::any_cast<std::string>(args[0]);
 *     std::string message = std::any_cast<std::string>(args[1]);
 *     sendActualEmail(email, message);
 * });
 * 
 * // Bind to engine
 * engine.bindActionsToState();
 * @endcode
 */

#pragma once

#include <string>
#include <any>
#include <functional>
#include <vector>
#include <unordered_map>

namespace fastrules {

/**
 * @brief Callback registration and invocation
 * 
 * Manages named callbacks that can be invoked from Lua actions.
 * Each callback receives a target object (usually the Workflow) and
 * a vector of arguments.
 * 
 * Handler Signature:
 * void(const std::any& target, const std::vector<std::any>& args)
 * 
 * The target is currently unused but reserved for future object-oriented
 * callbacks. The args vector contains converted Lua arguments.
 */
class ActionCallbacks {
public:
    /// @brief Handler function type
    using Handler = std::function<void(const std::any& target,
                                       const std::vector<std::any>& args)>;

    /// @brief Default constructor
    ActionCallbacks() = default;
    
    /// @brief Default destructor
    ~ActionCallbacks() = default;
    
    /// @brief Copy constructor
    ActionCallbacks(const ActionCallbacks&) = default;
    
    /// @brief Copy assignment
    ActionCallbacks& operator=(const ActionCallbacks&) = default;
    
    /// @brief Move constructor
    ActionCallbacks(ActionCallbacks&&) = default;
    
    /// @brief Move assignment
    ActionCallbacks& operator=(ActionCallbacks&&) = default;

    /**
     * @brief Register a callback handler
     * 
     * @param name The callback name (used as callbacks.name in Lua)
     * @param handler The handler function
     * 
     * Example:
     * @code
     * callbacks.registerHandler("log", [](const std::any&,
     *                                     const std::vector<std::any>& args) {
     *     std::string msg = std::any_cast<std::string>(args[0]);
     *     std::cout << msg << std::endl;
     * });
     * @endcode
     */
    void registerHandler(const std::string& name, Handler handler);

    /**
     * @brief Check if a handler is registered
     * 
     * @param name The callback name
     * @return true if registered, false otherwise
     */
    [[nodiscard]] bool hasHandler(const std::string& name) const;

    /**
     * @brief Get a registered handler
     * 
     * @param name The callback name
     * @return The handler function
     * @throws std::runtime_error if not found
     */
    [[nodiscard]] Handler getHandler(const std::string& name) const;

    /**
     * @brief Register a stub handler
     * 
     * Creates a no-op handler for the given name.
     * Used by discoverCallbacks for unknown callbacks.
     * 
     * @param name The callback name
     */
    void registerStub(const std::string& name) {
        handlers_[name] = [](const std::any&, const std::vector<std::any>&) {
            // No-op stub handler
        };
    }

    /**
     * @brief Check if a handler is registered
     * 
     * @param name The callback name
     * @return true if registered, false otherwise
     */
    [[nodiscard]] bool hasHandler(const std::string& name) const {
        return handlers_.find(name) != handlers_.end();
    }

    /**
     * @brief Iterate over all handlers
     * 
     * Calls the provided function for each registered handler.
     * 
     * @param func Callback receiving (name, handler) pairs
     */
    void forEachHandler(const std::function<void(const std::string&, const Handler&)>& func) const {
        for (const auto& [name, handler] : handlers_) {
            func(name, handler);
        }
    }

private:
    /// @brief Map of registered handlers
    std::unordered_map<std::string, Handler> handlers_;
};

} // namespace fastrules
