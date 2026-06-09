#pragma once

#include <any>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <typeindex>

struct lua_State;

namespace fastrules {

// Forward declarations for binding methods
class TypeRegistry;
class ActionCallbacks;
enum class LuaType {
    Nil,
    Boolean,
    Number,
    Integer,
    String,
    Table,
    Function,
    Userdata
};

// ============================================================================
// Lightweight Lua value wrapper — implemented by each backend
// ============================================================================
class LuaValue {
public:
    virtual ~LuaValue() = default;

    [[nodiscard]] virtual LuaType type() const = 0;
    [[nodiscard]] virtual bool isNil() const = 0;
    [[nodiscard]] virtual bool isString() const = 0;     // NEW
    [[nodiscard]] virtual bool isTable() const = 0;
    [[nodiscard]] virtual bool toBool() const = 0;
    [[nodiscard]] virtual double toNumber() const = 0;
    [[nodiscard]] virtual int64_t toInteger() const = 0;
    [[nodiscard]] virtual std::string toString() const = 0;
    [[nodiscard]] virtual size_t length() const = 0;                 // for tables/strings
    [[nodiscard]] virtual std::vector<std::string> keys() const = 0;    // for tables
    [[nodiscard]] virtual std::unique_ptr<LuaValue> get(const std::string& key) const = 0; // table access
    virtual void set(const std::string& key, const LuaValue& value) = 0;  // table mutation
};

// ============================================================================
// Callback signatures for C++ functions exposed to Lua
// ============================================================================
using LuaNativeFunc = std::function<std::vector<std::unique_ptr<LuaValue>>(lua_State*, const std::vector<std::unique_ptr<LuaValue>>&)>;
using LuaPredicateFunc = std::function<bool(lua_State*, const std::vector<std::unique_ptr<LuaValue>>&)>;

// ============================================================================
// Abstract LuaBackend interface
//
// Both Sol2Backend and LuaBridge3Backend implement this interface.
// The LuaEngine owns a std::unique_ptr<LuaBackend> and calls these methods.
// ============================================================================
class LuaBackend {
public:
    virtual ~LuaBackend() = default;

    // ── Compilation ─────────────────────────────────────────────────────────
    // Store compiled code under a string ID for later evaluation/execution.
    virtual void compileExpression(const std::string& id, const std::string& code) = 0;
    virtual void compileAction(const std::string& id, const std::string& code) = 0;

    // ── Execution ───────────────────────────────────────────────────────────
    // Evaluate a compiled expression with parameters. Returns the result.
    // Parameters are name→value pairs (std::any for RuleParameter compatibility).
    [[nodiscard]] virtual std::unique_ptr<LuaValue> evaluate(
        const std::string& id,
        const std::vector<std::pair<std::string, std::any>>& params) = 0;

    // Execute a compiled action with parameters.
    virtual void executeAction(
        const std::string& id,
        const std::vector<std::pair<std::string, std::any>>& params) = 0;

    // ── Coroutines ──────────────────────────────────────────────────────────
    // Create/resume/close a coroutine from a compiled expression.
    // Handle is opaque (void*) because the backend manages the concrete type.
    [[nodiscard]] virtual void* createCoroutine(const std::string& id) = 0;
    [[nodiscard]] virtual bool resumeCoroutine(void* handle) = 0;
    virtual void closeCoroutine(void* handle) = 0;

    // ── Reference lifecycle ─────────────────────────────────────────────────
    // Remove compiled code associated with an ID.
    virtual void removeCompiled(const std::string& id) = 0;

    // ── Globals ─────────────────────────────────────────────────────────────
    // Set/get/clear Lua global variables.
    virtual void setGlobal(const std::string& name, const LuaValue& value) = 0;
    [[nodiscard]] virtual std::unique_ptr<LuaValue> getGlobal(const std::string& name) = 0;
    virtual void clearGlobals() = 0;

    // ── Native function / predicate registry ────────────────────────────────
    // Register C++ functions/predicates callable from Lua.
    virtual void registerFunction(const std::string& name, LuaNativeFunc func) = 0;
    virtual void registerPredicate(const std::string& name, LuaPredicateFunc func) = 0;

    // ── State management ──────────────────────────────────────────────────
    virtual void openLibraries() = 0;
    [[nodiscard]] virtual lua_State* state() const = 0;     // raw lua_State*
    [[nodiscard]] virtual void* nativeState() const = 0;    // sol::state* for Sol2Backend, nullptr otherwise
    virtual void reset() = 0;
    virtual void collectGarbage() = 0;
    [[nodiscard]] virtual size_t memoryUsageKB() const = 0;

    // ── Value creation helpers ──────────────────────────────────────────────
    [[nodiscard]] virtual std::unique_ptr<LuaValue> makeNil() = 0;
    [[nodiscard]] virtual std::unique_ptr<LuaValue> makeBool(bool value) = 0;
    [[nodiscard]] virtual std::unique_ptr<LuaValue> makeInt(int value) = 0;
    [[nodiscard]] virtual std::unique_ptr<LuaValue> makeDouble(double value) = 0;
    [[nodiscard]] virtual std::unique_ptr<LuaValue> makeString(const std::string& value) = 0;
    [[nodiscard]] virtual std::unique_ptr<LuaValue> makeString(const char* value) = 0;

    // ── Pointer support (for TypeRegistry object parameters) ────────────────
    [[nodiscard]] virtual std::unique_ptr<LuaValue> makePointer(void* ptr) = 0;

    // ── Table creation ─────────────────────────────────────────────────────
    [[nodiscard]] virtual std::unique_ptr<LuaValue> createTable() = 0;

    // ── Type / Action binding (backend-specific, e.g. sol2 type registration) ──
    // Bind all registered C++ types to the Lua state. Called by LuaEngine::setupEnvironment().
    virtual void bindTypes(class TypeRegistry* registry) = 0;

    // Bind all registered action callbacks to the Lua state. Called by LuaEngine::setupEnvironment().
    virtual void bindActions(class ActionCallbacks* callbacks) = 0;

    // Set a registered C++ type as a global variable. Used for RuleParameter objects.
    virtual void setRegisteredTypeGlobal(const std::string& name, const std::type_index& type, const std::any& value, class TypeRegistry* registry) = 0;

    // Clear a registered type global (set to nil).
    virtual void clearRegisteredTypeGlobal(const std::string& name) = 0;

    // ── Factory ─────────────────────────────────────────────────────────────
    // Create the default backend (sol2 for now; CMake will select at compile time).
    [[nodiscard]] static std::unique_ptr<LuaBackend> create();
};

} // namespace fastrules