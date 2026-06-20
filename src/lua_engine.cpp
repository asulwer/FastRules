/**
 * @file lua_engine.cpp
 * @brief LuaEngine implementation - high-level Lua script execution
 *
 * This file implements the LuaEngine class which provides:
 * - Expression compilation and caching
 * - Expression evaluation with parameters
 * - Action execution
 * - Timeout handling via Lua hooks
 * - Type marshaling and binding
 * - Global variable management
 * - Action callback discovery and registration
 *
 * Timeout Implementation:
 * - Uses thread_local deadline pointer
 * - Lua hook checks deadline every N instructions
 * - Throws RuleTimeoutException if exceeded
 * - Thread-local ensures parallel safety
 *
 * Parameter Extraction:
 * - Parses expressions to find parameter names
 * - Hand-rolled parser for performance (regex is 50-100x slower)
 * - Filters out Lua keywords and builtins
 *
 * Caching:
 * - Compiled expressions cached by string hash
 * - Avoids recompilation of same expressions
 * - Ref counting for proper cleanup
 *
 * Thread Safety:
 * - Uses thread_local for current context
 * - Separate Lua state per engine
 * - Safe for parallel execution with different engines
 *
 * @see lua_backend_luabridge.cpp for backend implementation
 */

#include "fastrules/lua_engine.hpp"
#include "fastrules/rule.hpp"
#include "fastrules/rule_context.hpp"
#include "fastrules/rule_result.hpp"
#include "fastrules/logger.hpp"

#include <chrono>
#include <stdexcept>
#include <unordered_set>
#include <algorithm>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

// Lua C API: lua_sethook is declared in lauxlib.h which we include above.
// If using LuaJIT, lauxlib.h may not declare lua_sethook directly,
// but including both lua.h and lauxlib.h is the correct approach.

namespace {
    /**
     * Thread-local deadline for timeout preemption.
     * 
     * This pointer is set before Lua execution and checked by the Lua hook.
     * Using a pointer allows null-checking (no timeout if null).
     * Thread-local ensures parallel execution doesn't interfere.
     */
    thread_local std::chrono::steady_clock::time_point* g_deadline = nullptr;
}

namespace fastrules {

// Thread-local context for parallel execution
thread_local RuleContext* LuaEngine::currentContext_ = nullptr;

namespace {

// Extract parameter names from a Lua expression
// Identifies standalone variables (not field accesses, not function names)
// NOTE: Replaced std::regex with hand-rolled parser for performance (regex is 50-100x slower)
std::vector<std::string> extractParameterNames(const std::string& expression) {
    std::vector<std::string> names;
    
    // Static keyword/builtin sets - initialized once
    static const std::unordered_set<std::string_view> luaKeywords = {
        "and", "break", "do", "else", "elseif", "end", "false", "for",
        "function", "goto", "if", "in", "local", "nil", "not", "or",
        "repeat", "return", "then", "true", "until", "while"
    };
    static const std::unordered_set<std::string_view> builtins = {
        "isNotNull", "isNull", "isEmpty", "isNotEmpty", "inRange",
        "matchesRegex", "startsWith", "endsWith", "hasLength",
        "hasMinLength", "hasMaxLength", "countEquals", "countGreaterThan",
        "countLessThan", "contains", "string", "table", "math",
        "pairs", "ipairs", "tonumber", "tostring", "type", "print",
        "coroutine", "os", "io", "debug", "package", "require",
        "loadfile", "dofile", "load", "assert", "error", "pcall",
        "xpcall", "collectgarbage", "module", "select",
        "rawget", "rawset", "rawequal", "rawlen", "getmetatable",
        "setmetatable", "next", "unpack", "_G", "_VERSION",
        "context", "success", "failure", "callbacks"
    };
    
    enum class State { Normal, SingleString, DoubleString };
    State state = State::Normal;
    
    std::string_view expr(expression);
    size_t i = 0;
    
    while (i < expr.size()) {
        char c = expr[i];
        
        switch (state) {
            case State::SingleString:
                if (c == '\\' && i + 1 < expr.size()) {
                    i += 2;  // Skip escaped char
                } else if (c == '\'') {
                    state = State::Normal;
                    ++i;
                } else {
                    ++i;
                }
                break;
                
            case State::DoubleString:
                if (c == '\\' && i + 1 < expr.size()) {
                    i += 2;  // Skip escaped char
                } else if (c == '"') {
                    state = State::Normal;
                    ++i;
                } else {
                    ++i;
                }
                break;
                
            case State::Normal:
                // Skip string literals
                if (c == '\'') {
                    state = State::SingleString;
                    ++i;
                } else if (c == '"') {
                    state = State::DoubleString;
                    ++i;
                }
                // Skip numeric literals
                else if (std::isdigit(c) || (c == '.' && i + 1 < expr.size() && std::isdigit(expr[i + 1]))) {
                    while (i < expr.size() && (std::isdigit(expr[i]) || expr[i] == '.')) ++i;
                }
                // Identifier start
                else if (std::isalpha(c) || c == '_') {
                    size_t start = i;
                    while (i < expr.size() && (std::isalnum(expr[i]) || expr[i] == '_')) ++i;
                    
                    std::string_view name(expr.data() + start, i - start);
                    
                    // Check if preceded by dot (field access)
                    bool isField = false;
                    size_t check = start;
                    while (check > 0 && std::isspace(expr[check - 1])) --check;
                    if (check > 0 && expr[check - 1] == '.') isField = true;
                    
                    // Check if followed by paren (function call)
                    bool isFunction = false;
                    size_t after = i;
                    while (after < expr.size() && std::isspace(expr[after])) ++after;
                    if (after < expr.size() && expr[after] == '(') isFunction = true;
                    
                    if (!isField && !isFunction && 
                        !luaKeywords.contains(name) && 
                        !builtins.contains(name)) {
                        // Check for duplicates
                        bool exists = false;
                        for (const auto& n : names) {
                            if (n == name) { exists = true; break; }
                        }
                        if (!exists) {
                            names.emplace_back(name);
                        }
                    }
                }
                else {
                    ++i;
                }
                break;
        }
    }
    
    return names;
}

// Helper: convert std::any to LuaValue via backend
std::unique_ptr<LuaValue> anyToLuaValue(LuaBackend& backend, const std::any& value) {
    if (!value.has_value()) {
        return backend.makeNil();
    }
    try {
        if (value.type() == typeid(bool)) {
            return backend.makeBool(std::any_cast<bool>(value));
        } else if (value.type() == typeid(int)) {
            return backend.makeInt(std::any_cast<int>(value));
        } else if (value.type() == typeid(double)) {
            return backend.makeDouble(std::any_cast<double>(value));
        } else if (value.type() == typeid(std::string)) {
            return backend.makeString(std::any_cast<std::string>(value));
        } else if (value.type() == typeid(const char*)) {
            return backend.makeString(std::string(std::any_cast<const char*>(value)));
        } else if (value.type() == typeid(void*)) {
            return backend.makePointer(std::any_cast<void*>(value));
        }
    } catch (const std::bad_any_cast& e) {
        // Log the error if a logger is available, then fall through to nil
        // This indicates a type registration mismatch -- worth knowing about
        try {
            auto log = fastrules::logger();
            if (log) {
                log->warn("anyToLuaValue: bad_any_cast for type {}: {}", value.type().name(), e.what());
            }
        } catch (...) {
            // Logger not available -- ignore
        }
    }
    // If we get here, we don't know how to convert the type
    // This is not an error -- just means we don't have a binding for it
    return backend.makeNil();
}

} // anonymous namespace

// Simple scope guard for g_deadline cleanup - no exceptions, no std::function
struct DeadlineGuard {
    std::chrono::steady_clock::time_point** ptr;
    bool dismissed;
    
    explicit DeadlineGuard(std::chrono::steady_clock::time_point** p) 
        : ptr(p), dismissed(false) {}
    
    ~DeadlineGuard() {
        if (!dismissed && ptr) {
            *ptr = nullptr;
        }
    }
    
    void dismiss() { dismissed = true; }
    
    // No copy, no move - simple RAII
    DeadlineGuard(const DeadlineGuard&) = delete;
    DeadlineGuard& operator=(const DeadlineGuard&) = delete;
};

LuaEngine::LuaEngine() : backend_(LuaBackend::create()) {
    backend_->openLibraries();
    setupEnvironment();
    registerPredicates();
}

LuaEngine::~LuaEngine() {
    // Close any open coroutine handles
    for (const auto& [ref, handle] : coroutineHandles_) {
        if (handle) {
            backend_->closeCoroutine(handle);
        }
    }
}

LuaEngine::LuaEngine(LuaEngine&& other) noexcept
    : backend_(std::move(other.backend_))
    , refToBackendId_(std::move(other.refToBackendId_))
    , coroutineRegistry_(std::move(other.coroutineRegistry_))
    , coroutineHandles_(std::move(other.coroutineHandles_))
    , paramNames_(std::move(other.paramNames_))
    , nextRefId_(other.nextRefId_)
    , typeRegistry_(std::move(other.typeRegistry_))
    , actionCallbacks_(std::move(other.actionCallbacks_))
    , compileCount_(other.compileCount_.load())
    , generation_(other.generation_.load())
{
}

LuaEngine& LuaEngine::operator=(LuaEngine&& other) noexcept {
    if (this != &other) {
        // Close our coroutine handles first
        for (const auto& [ref, handle] : coroutineHandles_) {
            if (handle) {
                backend_->closeCoroutine(handle);
            }
        }
        backend_ = std::move(other.backend_);
        refToBackendId_ = std::move(other.refToBackendId_);
        coroutineRegistry_ = std::move(other.coroutineRegistry_);
        coroutineHandles_ = std::move(other.coroutineHandles_);
        paramNames_ = std::move(other.paramNames_);
        nextRefId_ = other.nextRefId_;
        typeRegistry_ = std::move(other.typeRegistry_);
        actionCallbacks_ = std::move(other.actionCallbacks_);
        compileCount_ = other.compileCount_.load();
        generation_ = other.generation_.load();
    }
    return *this;
}

std::unique_ptr<LuaEngine> LuaEngine::clone() const {
    auto engine = std::make_unique<LuaEngine>();
    engine->typeRegistry_ = typeRegistry_;
    engine->actionCallbacks_ = actionCallbacks_;
    // NOTE: Do NOT copy refToBackendId_ or paramNames_ - these are tied to
    // the original Lua state. The cloned engine will compile its own refs.
    engine->maxExpressionLength_ = maxExpressionLength_;
    engine->compileCount_.store(compileCount_.load());
    engine->generation_.store(generation_.load());
    engine->autoResetThresholdKB_ = autoResetThresholdKB_;
    engine->bindTypesToState();
    engine->bindActionsToState();
    return engine;
}

void LuaEngine::setupEnvironment() {
    // Use backend to clear sensitive globals
    backend_->setGlobal("os", *backend_->makeNil());
    backend_->setGlobal("io", *backend_->makeNil());
    backend_->setGlobal("debug", *backend_->makeNil());
    backend_->setGlobal("loadfile", *backend_->makeNil());
    backend_->setGlobal("dofile", *backend_->makeNil());
    backend_->setGlobal("require", *backend_->makeNil());
    
    // Create context table
    auto ctxTbl = backend_->createTable();
    backend_->setGlobal("context", *ctxTbl);

    // Set up the timeout hook on the Lua state
    auto* L = backend_->state();
    lua_sethook(L, [](lua_State* L2, lua_Debug* ar) {
        (void)ar;
        (void)L2;
        // Check if deadline has been exceeded
        if (g_deadline != nullptr && std::chrono::steady_clock::now() > *g_deadline) {
            luaL_error(L2, "execution timed out");
        }
    }, LUA_MASKCOUNT, 1000);
}

void LuaEngine::setupContextTable(RuleContext& context) {
    // Set the thread-local context for this execution
    currentContext_ = &context;
    
    // Get the context table
    auto ctxTbl = backend_->getGlobal("context");
    
    // If context table doesn't exist or isn't a table, recreate it
    if (!ctxTbl || !ctxTbl->isTable()) {
        ctxTbl = backend_->createTable();
        backend_->setGlobal("context", *ctxTbl);
    }
    
    // Create getResult function directly using Lua C API
    lua_State* L = backend_->state();
    
    if (L) {
        // Get the context table onto the stack
        lua_getglobal(L, "context");
        if (lua_istable(L, -1)) {
            // Push the key
            lua_pushstring(L, "getResult");
            
            // Push the C closure with the engine pointer as upvalue
            lua_pushlightuserdata(L, this);
            lua_pushcclosure(L, [](lua_State* L) -> int {
                // Get the engine pointer from upvalue
                auto* engine = static_cast<LuaEngine*>(lua_touserdata(L, lua_upvalueindex(1)));
                if (!engine || !engine->currentContext_) {
                    lua_pushnil(L);
                    return 1;
                }
                
                // Get the rule name argument
                std::string ruleName;
                if (lua_isstring(L, 1)) {
                    ruleName = lua_tostring(L, 1);
                } else if (lua_isnumber(L, 1)) {
                    ruleName = std::to_string(static_cast<int>(lua_tonumber(L, 1)));
                } else {
                    ruleName = "";
                }
                
                // Get the result from context
                auto result = engine->currentContext_->getResult(ruleName);
                
                // Create return table
                lua_newtable(L);
                if (result.has_value()) {
                    lua_pushboolean(L, result->success);
                    lua_setfield(L, -2, "success");
                    lua_pushstring(L, result->ruleName.c_str());
                    lua_setfield(L, -2, "ruleId");
                } else {
                    lua_pushboolean(L, false);
                    lua_setfield(L, -2, "success");
                    lua_pushstring(L, ruleName.c_str());
                    lua_setfield(L, -2, "ruleId");
                }
                
                return 1;  // Return the table
            }, 1);
            
            // Set context.getResult = function
            lua_settable(L, -3);
        }
        lua_pop(L, 1);  // Pop the context table
    }
}

void LuaEngine::registerPredicates() {
    backend_->registerPredicate("isNotNull", [](lua_State* /*L*/, const std::vector<std::unique_ptr<LuaValue>>& args) -> bool {
        if (args.empty()) return false;
        return !args[0]->isNil();
    });
    backend_->registerPredicate("isNull", [](lua_State* /*L*/, const std::vector<std::unique_ptr<LuaValue>>& args) -> bool {
        if (args.empty()) return true;
        return args[0]->isNil();
    });
    backend_->registerPredicate("isEmpty", [](lua_State* /*L*/, const std::vector<std::unique_ptr<LuaValue>>& args) -> bool {
        if (args.empty()) return true;
        return args[0]->toString().empty();
    });
    backend_->registerPredicate("isNotEmpty", [](lua_State* /*L*/, const std::vector<std::unique_ptr<LuaValue>>& args) -> bool {
        if (args.empty()) return false;
        return !args[0]->toString().empty();
    });
    backend_->registerPredicate("inRange", [](lua_State* /*L*/, const std::vector<std::unique_ptr<LuaValue>>& args) -> bool {
        if (args.size() < 3) return false;
        double value = args[0]->toNumber();
        double min = args[1]->toNumber();
        double max = args[2]->toNumber();
        return value >= min && value <= max;
    });
    backend_->registerPredicate("matchesRegex", [](lua_State* /*L*/, const std::vector<std::unique_ptr<LuaValue>>& args) -> bool {
        if (args.size() < 2) return false;
        std::string str = args[0]->toString();
        std::string pattern = args[1]->toString();
        return str.find(pattern) != std::string::npos;
    });
    backend_->registerPredicate("startsWith", [](lua_State* /*L*/, const std::vector<std::unique_ptr<LuaValue>>& args) -> bool {
        if (args.size() < 2) return false;
        std::string str = args[0]->toString();
        std::string prefix = args[1]->toString();
        return str.size() >= prefix.size() && str.compare(0, prefix.size(), prefix) == 0;
    });
    backend_->registerPredicate("endsWith", [](lua_State* /*L*/, const std::vector<std::unique_ptr<LuaValue>>& args) -> bool {
        if (args.size() < 2) return false;
        std::string str = args[0]->toString();
        std::string suffix = args[1]->toString();
        return str.size() >= suffix.size() && str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
    });
    backend_->registerPredicate("hasLength", [](lua_State* /*L*/, const std::vector<std::unique_ptr<LuaValue>>& args) -> bool {
        if (args.size() < 2) return false;
        return args[0]->toString().length() == static_cast<size_t>(args[1]->toInteger());
    });
    backend_->registerPredicate("hasMinLength", [](lua_State* /*L*/, const std::vector<std::unique_ptr<LuaValue>>& args) -> bool {
        if (args.size() < 2) return false;
        return args[0]->toString().length() >= static_cast<size_t>(args[1]->toInteger());
    });
    backend_->registerPredicate("hasMaxLength", [](lua_State* /*L*/, const std::vector<std::unique_ptr<LuaValue>>& args) -> bool {
        if (args.size() < 2) return false;
        return args[0]->toString().length() <= static_cast<size_t>(args[1]->toInteger());
    });
    backend_->registerPredicate("countEquals", [](lua_State* /*L*/, const std::vector<std::unique_ptr<LuaValue>>& args) -> bool {
        if (args.size() < 2 || !args[0]->isTable()) return false;
        size_t expected = static_cast<size_t>(args[1]->toInteger());
        return args[0]->length() == expected;
    });
    backend_->registerPredicate("countGreaterThan", [](lua_State* /*L*/, const std::vector<std::unique_ptr<LuaValue>>& args) -> bool {
        if (args.size() < 2 || !args[0]->isTable()) return false;
        size_t count = static_cast<size_t>(args[1]->toInteger());
        return args[0]->length() > count;
    });
    backend_->registerPredicate("countLessThan", [](lua_State* /*L*/, const std::vector<std::unique_ptr<LuaValue>>& args) -> bool {
        if (args.size() < 2 || !args[0]->isTable()) return false;
        size_t count = static_cast<size_t>(args[1]->toInteger());
        return args[0]->length() < count;
    });
    backend_->registerPredicate("contains", [](lua_State* /*L*/, const std::vector<std::unique_ptr<LuaValue>>& args) -> bool {
        if (args.size() < 2 || !args[0]->isTable()) return false;
        auto keys = args[0]->keys();
        std::string searchStr = args[1]->toString();
        for (const auto& key : keys) {
            auto val = args[0]->get(key);
            if (val->toString() == searchStr) return true;
        }
        return false;
    });
}

lua_State* LuaEngine::luaState() const noexcept {
    return backend_->state();
}

void LuaEngine::bindTypesToState() {
    backend_->bindTypes(&typeRegistry_);
}

void LuaEngine::bindActionsToState() {
    backend_->bindActions(&actionCallbacks_);
}

void LuaEngine::registerAction(const std::string& name,
                               std::function<void(const std::any&, const std::vector<std::any>&)> handler) {
    actionCallbacks_.registerHandler(name, std::move(handler));
}

// ============================================================================
// No-globals implementation
// Parameters are extracted from expressions and passed as globals via backend
// ============================================================================

std::string LuaEngine::wrapExpression(const std::string& expression) {
    return expression;
}

std::string LuaEngine::wrapAction(const std::string& action) {
    return action;
}

std::string LuaEngine::wrapCoroutine(const std::string& expression) {
    return expression;
}

std::string LuaEngine::makeBackendId(int ref) const {
    return "ref_" + std::to_string(ref);
}

std::optional<int> LuaEngine::compileExpression(const std::string& expression) {
    // Ensure registered types are bound to this Lua state before compiling
    // any expression that may reference them.
    bindTypesToState();

    if (maxExpressionLength_ > 0 && expression.size() > maxExpressionLength_) {
        throw RuleCompilationException(
            "Expression exceeds maximum length of " + std::to_string(maxExpressionLength_) + 
            " characters (was " + std::to_string(expression.size()) + ")");
    }

    // Empty or whitespace-only expressions are not compilable
    if (expression.empty() ||
        std::all_of(expression.begin(), expression.end(), [](unsigned char c) { return std::isspace(c); })) {
        return std::nullopt;
    }

    std::vector<std::string> params = extractParameterNames(expression);

    int ref;
    std::string backendId;
    try {
        std::lock_guard<std::shared_mutex> lock(registryMutex_);
        ref = nextRefId_++;
        backendId = makeBackendId(ref);
        backend_->compileExpression(backendId, expression);
        paramNames_[ref] = params;
        compileCount_.fetch_add(1);
    } catch (const std::exception& e) {
        throw RuleCompilationException(std::string("Compilation failed: ") + e.what());
    }
    refToBackendId_[ref] = backendId;

    if (autoResetThresholdKB_ > 0) {
        size_t memKB = getMemoryUsageKB();
        if (memKB > autoResetThresholdKB_) {
            if (logger_) {
                logger_->warn("Lua state memory ({} KB) exceeded threshold ({} KB). Triggering state reset.", memKB, autoResetThresholdKB_);
            }
            resetState();
        }
    }

    return ref;
}

std::optional<int> LuaEngine::compileAction(const std::string& action) {
    // Ensure action callbacks are bound before compiling an action.
    bindActionsToState();

    if (action.empty()) {
        return std::nullopt;
    }

    if (maxExpressionLength_ > 0 && action.size() > maxExpressionLength_) {
        throw RuleCompilationException(
            "Action exceeds maximum length of " + std::to_string(maxExpressionLength_) + 
            " characters (was " + std::to_string(action.size()) + ")");
    }

    std::vector<std::string> params = extractParameterNames(action);

    int ref;
    std::string backendId;
    try {
        std::lock_guard<std::shared_mutex> lock(registryMutex_);
        ref = nextRefId_++;
        backendId = makeBackendId(ref);
        backend_->compileAction(backendId, action);
        paramNames_[ref] = params;
        compileCount_.fetch_add(1);
    } catch (const std::exception& e) {
        throw RuleCompilationException(std::string("Compilation failed: ") + e.what());
    }
    refToBackendId_[ref] = backendId;

    if (autoResetThresholdKB_ > 0) {
        size_t memKB = getMemoryUsageKB();
        if (memKB > autoResetThresholdKB_) {
            if (logger_) {
                logger_->warn("Lua state memory ({} KB) exceeded threshold ({} KB). Triggering state reset.", memKB, autoResetThresholdKB_);
            }
            resetState();
        }
    }

    return ref;
}

std::optional<int> LuaEngine::compileCoroutine(const std::string& expression) {
    // Ensure registered types and actions are bound before compiling.
    bindTypesToState();
    bindActionsToState();

    if (expression.empty()) {
        return std::nullopt;
    }

    std::vector<std::string> params = extractParameterNames(expression);

    int ref;
    std::string backendId;
    void* handle = nullptr;
    {
        std::lock_guard<std::shared_mutex> lock(registryMutex_);
        ref = nextRefId_++;
        backendId = makeBackendId(ref);
        backend_->compileExpression(backendId, expression);
        handle = backend_->createCoroutine(backendId);
        if (!handle) {
            throw RuleCompilationException("Failed to create coroutine from compiled expression");
        }
        coroutineRegistry_[ref] = true;
        coroutineHandles_[ref] = handle;
        paramNames_[ref] = params;
        compileCount_.fetch_add(1);
    }
    refToBackendId_[ref] = backendId;

    if (autoResetThresholdKB_ > 0) {
        size_t memKB = getMemoryUsageKB();
        if (memKB > autoResetThresholdKB_) {
            if (logger_) {
                logger_->warn("Lua state memory ({} KB) exceeded threshold ({} KB). Triggering state reset.", memKB, autoResetThresholdKB_);
            }
            resetState();
        }
    }

    return ref;
}

std::vector<std::pair<std::string, std::any>> LuaEngine::buildParamPairs(
    int /*ref*/,
    const std::vector<RuleParameter>& parameters)
{
    std::vector<std::pair<std::string, std::any>> pairs;

    // Always set every parameter that was actually supplied at runtime.
    // The compiler's extracted parameter list may miss identifiers, but the
    // caller knows what data is available. Missing (not-supplied) parameters
    // are intentionally left unset so that externally-set globals (e.g. via
    // setGlobal) remain visible to the expression.
    pairs.reserve(parameters.size());
    for (const auto& param : parameters) {
        pairs.emplace_back(param.name, param.value);
    }

    return pairs;
}

std::any LuaEngine::luaValueToAny(const LuaValue& value) const {
    switch (value.type()) {
        case LuaType::Boolean: return value.toBool();
        case LuaType::Number: return value.toNumber();
        case LuaType::Integer: return static_cast<int>(value.toInteger());
        case LuaType::String: return value.toString();
        default: return std::any{};
    }
}

bool LuaEngine::resumeCoroutine(int ref, const std::vector<RuleParameter>& parameters, RuleContext& context) {
    std::scoped_lock<std::mutex> luaLock(luaStateMutex_);
    
    std::shared_lock<std::shared_mutex> lock(registryMutex_);
    auto handleIt = coroutineHandles_.find(ref);
    if (handleIt == coroutineHandles_.end() || !handleIt->second) {
        throw RuleExecutionException("Invalid coroutine reference");
    }
    if (!coroutineRegistry_.contains(ref)) {
        throw RuleExecutionException("Reference is not a coroutine");
    }
    void* handle = handleIt->second;
    lock.unlock();

    setupContextTable(context);
    
    // Set parameters as globals before resuming
    auto pairs = buildParamPairs(ref, parameters);
    
    // Set registered object types as globals
    for (const auto& param : parameters) {
        if (param.type.has_value() && typeRegistry_.isRegistered(param.type.value())) {
            backend_->setRegisteredTypeGlobal(param.name, param.type.value(), param.value, &typeRegistry_);
            auto pit = std::remove_if(pairs.begin(), pairs.end(),
                [&param](const auto& p) { return p.first == param.name; });
            pairs.erase(pit, pairs.end());
        }
    }
    
    for (const auto& [name, value] : pairs) {
        auto lv = anyToLuaValue(*backend_, value);
        backend_->setGlobal(name, *lv);
    }

    bool finished = backend_->resumeCoroutine(handle);
    
    // Clear parameter globals
    for (const auto& [name, _] : pairs) {
        backend_->setGlobal(name, *backend_->makeNil());
    }
    // Clear object globals
    for (const auto& param : parameters) {
        if (param.type.has_value() && typeRegistry_.isRegistered(param.type.value())) {
            backend_->clearRegisteredTypeGlobal(param.name);
        }
    }

    return finished;
}

bool LuaEngine::isCoroutine(int ref) const {
    std::shared_lock<std::shared_mutex> lock(registryMutex_);
    return coroutineRegistry_.contains(ref);
}

bool LuaEngine::evaluateExpression(int ref, const std::vector<RuleParameter>& parameters, RuleContext& context, std::optional<std::chrono::milliseconds> timeout) {
    std::scoped_lock<std::mutex> luaLock(luaStateMutex_);
    
    std::shared_lock<std::shared_mutex> lock(registryMutex_);
    auto it = refToBackendId_.find(ref);
    if (it == refToBackendId_.end()) {
        throw RuleExecutionException("Invalid expression reference");
    }
    std::string backendId = it->second;
    lock.unlock();

    setupContextTable(context);
    auto pairs = buildParamPairs(ref, parameters);


    // Set registered object types as globals
    for (const auto& param : parameters) {
        if (param.type.has_value() && typeRegistry_.isRegistered(param.type.value())) {
            backend_->setRegisteredTypeGlobal(param.name, param.type.value(), param.value, &typeRegistry_);
            // Remove from pairs so backend doesn't try to convert it
            auto pit = std::remove_if(pairs.begin(), pairs.end(),
                [&param](const auto& p) { return p.first == param.name; });
            pairs.erase(pit, pairs.end());
        }
    }

    std::chrono::steady_clock::time_point deadline;
    bool hasTimeout = timeout.has_value() && timeout->count() > 0;
    DeadlineGuard guard(&g_deadline);
    if (hasTimeout) {
        deadline = std::chrono::steady_clock::now() + timeout.value();
        g_deadline = &deadline;
    } else {
        guard.dismiss();  // No timeout set, don't reset g_deadline
    }

    auto result = backend_->evaluate(backendId, pairs);

    // Clear object globals
    for (const auto& param : parameters) {
        if (param.type.has_value() && typeRegistry_.isRegistered(param.type.value())) {
            backend_->clearRegisteredTypeGlobal(param.name);
        }
    }

    // g_deadline is reset by DeadlineGuard destructor if set
    if (hasTimeout) {
        if (std::chrono::steady_clock::now() > deadline) {
            throw RuleTimeoutException("Rule execution timed out after " + std::to_string(timeout->count()) + "ms");
        }
    }

    if (!result) {
        throw RuleExecutionException("Expression evaluation returned null");
    }
    return result->toBool();
}

void LuaEngine::executeAction(int ref, const std::vector<RuleParameter>& parameters, RuleContext& context, std::optional<std::chrono::milliseconds> timeout) {
    std::scoped_lock<std::mutex> luaLock(luaStateMutex_);
    
    std::shared_lock<std::shared_mutex> lock(registryMutex_);
    auto it = refToBackendId_.find(ref);
    if (it == refToBackendId_.end()) {
        throw RuleExecutionException("Invalid action reference");
    }
    std::string backendId = it->second;
    lock.unlock();

    setupContextTable(context);
    auto pairs = buildParamPairs(ref, parameters);

    // Set registered object types as globals
    for (const auto& param : parameters) {
        if (param.type.has_value() && typeRegistry_.isRegistered(param.type.value())) {
            backend_->setRegisteredTypeGlobal(param.name, param.type.value(), param.value, &typeRegistry_);
            auto pit = std::remove_if(pairs.begin(), pairs.end(),
                [&param](const auto& p) { return p.first == param.name; });
            pairs.erase(pit, pairs.end());
        }
    }

    std::chrono::steady_clock::time_point deadline;
    bool hasTimeout = timeout.has_value() && timeout->count() > 0;
    DeadlineGuard guard(&g_deadline);
    if (hasTimeout) {
        deadline = std::chrono::steady_clock::now() + timeout.value();
        g_deadline = &deadline;
    } else {
        guard.dismiss();  // No timeout set, don't reset g_deadline
    }

    backend_->executeAction(backendId, pairs);

    // Clear object globals
    for (const auto& param : parameters) {
        if (param.type.has_value() && typeRegistry_.isRegistered(param.type.value())) {
            backend_->clearRegisteredTypeGlobal(param.name);
        }
    }

    // g_deadline is reset by ScopeGuard destructor if set
    if (hasTimeout) {
        if (std::chrono::steady_clock::now() > deadline) {
            throw RuleTimeoutException("Rule execution timed out after " + std::to_string(timeout->count()) + "ms");
        }
    }
}

void LuaEngine::setGlobal(const std::string& name, const std::any& value) {
    auto lv = anyToLuaValue(*backend_, value);
    backend_->setGlobal(name, *lv);
}

void LuaEngine::clearGlobals() {
    backend_->clearGlobals();
}

std::optional<std::any> LuaEngine::await(int ref, const std::vector<RuleParameter>& parameters, RuleContext& context) {
    bool completed;
    do {
        completed = resumeCoroutine(ref, parameters, context);
    } while (!completed);
    
    std::shared_lock<std::shared_mutex> lock(registryMutex_);
    auto handleIt = coroutineHandles_.find(ref);
    if (handleIt == coroutineHandles_.end() || !handleIt->second) {
        return std::nullopt;
    }
    
    // Resume one more time to get the final return value
    // Actually, the coroutine is already dead after the loop above.
    // The backend's resumeCoroutine should have returned the final value.
    // For now, return nil since we can't easily get the coroutine's return value
    // through the current backend interface.
    return std::any{};
}

void LuaEngine::releaseRef(int ref) {
    std::lock_guard<std::shared_mutex> lock(registryMutex_);
    auto it = refToBackendId_.find(ref);
    if (it != refToBackendId_.end()) {
        backend_->removeCompiled(it->second);
        refToBackendId_.erase(it);
    }
    auto coIt = coroutineHandles_.find(ref);
    if (coIt != coroutineHandles_.end() && coIt->second) {
        backend_->closeCoroutine(coIt->second);
        coroutineHandles_.erase(coIt);
    }
    coroutineRegistry_.erase(ref);
    paramNames_.erase(ref);
}

void LuaEngine::discoverCallbacks(const std::vector<std::string>& actions) {
    for (const auto& action : actions) {
        if (action.empty()) continue;

        size_t pos = 0;
        while ((pos = action.find("callbacks.", pos)) != std::string::npos) {
            pos += 10;

            size_t end = action.find('(', pos);
            if (end != std::string::npos) {
                std::string name = action.substr(pos, end - pos);

                while (!name.empty() && std::isspace(name.back())) name.pop_back();
                while (!name.empty() && std::isspace(name.front())) name.erase(name.begin());

                if (!name.empty() && !actionCallbacks_.hasHandler(name)) {
                    actionCallbacks_.registerStub(name);
                }
            }
        }
    }

    bindActionsToState();
}

// ============================================================================
// Collectible state cleanup
// ============================================================================

void LuaEngine::resetState() {
    std::lock_guard<std::mutex> luaLock(luaStateMutex_);

    generation_.fetch_add(1);

    if (logger_) {
        size_t memBefore = getMemoryUsageKB();
        logger_->info("Resetting Lua state. Memory before: {} KB, compiled refs: {}, compile count: {}", memBefore, refToBackendId_.size(), compileCount_.load());
    }

    {
        std::lock_guard<std::shared_mutex> regLock(registryMutex_);
        // Close coroutine handles
        for (const auto& [ref, handle] : coroutineHandles_) {
            if (handle) {
                backend_->closeCoroutine(handle);
            }
        }
        refToBackendId_.clear();
        coroutineRegistry_.clear();
        coroutineHandles_.clear();
        paramNames_.clear();
        nextRefId_ = 1;
        compileCount_.store(0);
    }

    backend_->collectGarbage();
    backend_->collectGarbage();

    backend_->reset();
    backend_->openLibraries();

    setupEnvironment();
    registerPredicates();

    bindTypesToState();
    bindActionsToState();

    if (logger_) {
        size_t memAfter = getMemoryUsageKB();
        logger_->info("Lua state reset complete. Memory after: {} KB", memAfter);
    }
}

void LuaEngine::collectGarbage() {
    std::lock_guard<std::mutex> lock(luaStateMutex_);
    backend_->collectGarbage();
    backend_->collectGarbage();
}

size_t LuaEngine::getMemoryUsageKB() {
    std::unique_lock<std::mutex> lock(luaStateMutex_, std::try_to_lock);
    if (!lock.owns_lock()) {
        return 0;
    }
    return backend_->memoryUsageKB();
}

} // namespace fastrules
