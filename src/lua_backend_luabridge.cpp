#include "fastrules/lua_backend_luabridge.hpp"
#include "fastrules/lua_backend.hpp"
#include "fastrules/type_registry.hpp"
#include "fastrules/action_callback.hpp"
#include <lua.hpp>
#include <LuaBridge/LuaBridge.h>
#include <any>
#include <cstring>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <new>

namespace fastrules {

// Forward declaration of LuaBridge3Backend for predicate handler
class LuaBridge3Backend;

// Static helper to call predicates - defined after the class
static int luaPredicateCallHandler(lua_State* L);

// ============================================================================
// LuaBridgeValue -- wraps a Lua registry reference as LuaValue
// ============================================================================
class LuaBridgeValue : public LuaValue {
public:
    LuaBridgeValue() : L_(nullptr), ref_(LUA_REFNIL) {}

    /**
     * @param L          the Lua state the value lives in
     * @param stackIndex stack slot to take a registry reference to
     * @param alive      liveness flag shared with the owning backend. The
     *                   backend clears it immediately before lua_close(), so a
     *                   LuaValue that outlives a resetState() degrades to nil
     *                   instead of unref-ing a destroyed state.
     */
    explicit LuaBridgeValue(lua_State* L, int stackIndex, std::shared_ptr<const bool> alive = nullptr)
        : L_(L), alive_(std::move(alive)) {
        if (L_) {
            lua_pushvalue(L_, stackIndex);
            ref_ = luaL_ref(L_, LUA_REGISTRYINDEX);
        } else {
            ref_ = LUA_REFNIL;
        }
    }

    ~LuaBridgeValue() override {
        if (valid()) {
            luaL_unref(L_, LUA_REGISTRYINDEX, ref_);
        }
    }

    // Move constructor / assignment
    LuaBridgeValue(LuaBridgeValue&& other) noexcept
        : L_(other.L_), ref_(other.ref_), alive_(std::move(other.alive_)) {
        other.ref_ = LUA_NOREF;
    }

    LuaBridgeValue& operator=(LuaBridgeValue&& other) noexcept {
        if (this != &other) {
            if (valid()) {
                luaL_unref(L_, LUA_REGISTRYINDEX, ref_);
            }
            L_ = other.L_;
            ref_ = other.ref_;
            alive_ = std::move(other.alive_);
            other.ref_ = LUA_NOREF;
        }
        return *this;
    }

    // Disable copy
    LuaBridgeValue(const LuaBridgeValue&) = delete;
    LuaBridgeValue& operator=(const LuaBridgeValue&) = delete;

    /// True when the referenced Lua state is still open and the ref is real.
    [[nodiscard]] bool valid() const {
        return L_ != nullptr
            && ref_ != LUA_REFNIL
            && ref_ != LUA_NOREF
            && (!alive_ || *alive_);
    }

    void push() const {
        if (!valid()) {
            if (L_ && (!alive_ || *alive_)) lua_pushnil(L_);
        } else {
            lua_rawgeti(L_, LUA_REGISTRYINDEX, ref_);
        }
    }

    [[nodiscard]] LuaType type() const override {
        if (!valid()) return LuaType::Nil;
        push();
        int t = lua_type(L_, -1);
        lua_pop(L_, 1);
        switch (t) {
            case LUA_TNIL: return LuaType::Nil;
            case LUA_TBOOLEAN: return LuaType::Boolean;
            case LUA_TNUMBER: {
                // Check if it's an integer
                push();
                if (lua_isinteger(L_, -1)) {
                    lua_pop(L_, 1);
                    return LuaType::Integer;
                }
                lua_pop(L_, 1);
                return LuaType::Number;
            }
            case LUA_TSTRING: return LuaType::String;
            case LUA_TTABLE: return LuaType::Table;
            case LUA_TFUNCTION: return LuaType::Function;
            default: return LuaType::Userdata;
        }
    }

    [[nodiscard]] bool isNil() const override {
        if (!valid()) return true;
        push();
        bool result = lua_isnil(L_, -1);
        lua_pop(L_, 1);
        return result;
    }

    [[nodiscard]] bool isString() const override {
        if (!valid()) return false;
        push();
        bool result = lua_isstring(L_, -1) != 0;
        lua_pop(L_, 1);
        return result;
    }

    [[nodiscard]] bool isTable() const override {
        if (!valid()) return false;
        push();
        bool result = lua_istable(L_, -1);
        lua_pop(L_, 1);
        return result;
    }

    [[nodiscard]] bool toBool() const override {
        if (!valid()) return false;
        push();
        bool result = lua_toboolean(L_, -1);
        lua_pop(L_, 1);
        return result;
    }

    [[nodiscard]] double toNumber() const override {
        if (!valid()) return 0.0;
        push();
        double result = 0.0;
        if (lua_isnumber(L_, -1)) {
            result = lua_tonumber(L_, -1);
        }
        lua_pop(L_, 1);
        return result;
    }

    [[nodiscard]] int64_t toInteger() const override {
        if (!valid()) return 0;
        push();
        int64_t result = 0;
        if (lua_isinteger(L_, -1)) {
            result = lua_tointeger(L_, -1);
        } else if (lua_isnumber(L_, -1)) {
            result = static_cast<int64_t>(lua_tonumber(L_, -1));
        }
        lua_pop(L_, 1);
        return result;
    }

    [[nodiscard]] std::string toString() const override {
        if (!valid()) return "";
        push();
        std::string result;
        if (lua_isstring(L_, -1)) {
            const char* str = lua_tostring(L_, -1);
            if (str) result = str;
        }
        lua_pop(L_, 1);
        return result;
    }

    [[nodiscard]] size_t length() const override {
        if (!valid()) return 0;
        push();
        size_t len = 0;
        if (lua_istable(L_, -1)) {
            // Count table entries
            lua_pushnil(L_);
            while (lua_next(L_, -2) != 0) {
                ++len;
                lua_pop(L_, 1); // pop value, keep key
            }
        } else if (lua_isstring(L_, -1)) {
            len = lua_rawlen(L_, -1);
        }
        lua_pop(L_, 1);
        return len;
    }

    [[nodiscard]] std::vector<std::string> keys() const override {
        std::vector<std::string> result;
        if (!valid()) return result;
        push();
        if (!lua_istable(L_, -1)) {
            lua_pop(L_, 1);
            return result;
        }
        lua_pushnil(L_);
        while (lua_next(L_, -2) != 0) {
            if (lua_isstring(L_, -2)) {
                const char* key = lua_tostring(L_, -2);
                if (key) result.push_back(key);
            }
            lua_pop(L_, 1); // pop value, keep key
        }
        lua_pop(L_, 1); // pop table
        return result;
    }

    [[nodiscard]] std::unique_ptr<LuaValue> get(const std::string& key) const override {
        if (!valid()) {
            return std::make_unique<LuaBridgeValue>();
        }
        push();
        if (!lua_istable(L_, -1)) {
            lua_pop(L_, 1);
            return std::make_unique<LuaBridgeValue>();
        }
        lua_getfield(L_, -1, key.c_str());
        auto val = std::make_unique<LuaBridgeValue>(L_, -1, alive_);
        lua_pop(L_, 2); // pop value and table
        return val;
    }

    void set(const std::string& key, const LuaValue& value) override {
        if (!valid()) return;
        push();
        if (!lua_istable(L_, -1)) {
            lua_pop(L_, 1);
            return;
        }
        if (auto* lbVal = dynamic_cast<const LuaBridgeValue*>(&value)) {
            lbVal->push();
        } else {
            lua_pushnil(L_);
        }
        lua_setfield(L_, -2, key.c_str());
        lua_pop(L_, 1); // pop table
    }

    [[nodiscard]] int ref() const { return ref_; }
    [[nodiscard]] lua_State* luaState() const { return L_; }
    [[nodiscard]] const std::shared_ptr<const bool>& aliveFlag() const { return alive_; }

private:
    lua_State* L_;
    int ref_;
    /// Cleared by the owning backend immediately before lua_close(), so that a
    /// value outliving a reset() cannot touch a destroyed Lua state.
    std::shared_ptr<const bool> alive_;
};

// ============================================================================
// Helper: push std::any onto Lua stack
// ============================================================================
static void pushAny(lua_State* L, const std::any& value) {
    if (!value.has_value()) {
        lua_pushnil(L);
        return;
    }
    const std::type_info& type = value.type();
    if (type == typeid(bool)) {
        lua_pushboolean(L, std::any_cast<bool>(value));
    } else if (type == typeid(int)) {
        lua_pushinteger(L, std::any_cast<int>(value));
    } else if (type == typeid(unsigned int)) {
        lua_pushinteger(L, static_cast<lua_Integer>(std::any_cast<unsigned int>(value)));
    } else if (type == typeid(long)) {
        lua_pushinteger(L, static_cast<lua_Integer>(std::any_cast<long>(value)));
    } else if (type == typeid(long long)) {
        lua_pushinteger(L, static_cast<lua_Integer>(std::any_cast<long long>(value)));
    } else if (type == typeid(int64_t)) {
        lua_pushinteger(L, static_cast<lua_Integer>(std::any_cast<int64_t>(value)));
    } else if (type == typeid(unsigned long)) {
        lua_pushinteger(L, static_cast<lua_Integer>(std::any_cast<unsigned long>(value)));
    } else if (type == typeid(unsigned long long)) {
        lua_pushinteger(L, static_cast<lua_Integer>(std::any_cast<unsigned long long>(value)));
    } else if (type == typeid(double)) {
        lua_pushnumber(L, std::any_cast<double>(value));
    } else if (type == typeid(float)) {
        lua_pushnumber(L, static_cast<lua_Number>(std::any_cast<float>(value)));
    } else if (type == typeid(std::string)) {
        const std::string& str = std::any_cast<const std::string&>(value);
        lua_pushlstring(L, str.c_str(), str.length());
    } else if (type == typeid(const char*)) {
        lua_pushstring(L, std::any_cast<const char*>(value));
    } else {
        lua_pushnil(L);
    }
}

// ============================================================================
// LuaBridge3Backend implementation
// ============================================================================
// Registry key (its address) under which the backend pointer is stashed.
// Using the registry rather than a global keeps it out of reach of Lua code,
// which could otherwise overwrite or delete a plain global.
static const char kBackendPtrKey = 0;

struct LuaBridge3Backend::Impl {
    lua_State* L = nullptr;
    std::unordered_map<std::string, int> compiled_;     // id -> registry ref
    std::unordered_map<int, lua_State*> coroutines_;     // registry ref -> thread
    std::unordered_set<std::string> globals_;
    std::unordered_map<std::string, LuaNativeFunc> nativeFuncs_;
    std::unordered_map<std::string, LuaPredicateFunc> predicates_;
    std::vector<ActionCallbacks::Handler> actionHandlers_;
    // name -> slot in actionHandlers_. Without this, every bindActions() call
    // appended a fresh copy of every handler and the vector grew without bound.
    std::unordered_map<std::string, int> actionHandlerIds_;
    // Stable copies of bound TypeMethods, keyed "<metatable>::<method>".
    // Metatables hold raw pointers to these as light userdata, so they must
    // not move. Pointing directly at TypeRegistry's vector was unsafe:
    // re-registering a type replaces that vector and leaves the metatable
    // holding dangling pointers.
    std::unordered_map<std::string, std::unique_ptr<TypeMethod>> boundMethods_;
    // Shared with every LuaBridgeValue handed out; set to false just before
    // lua_close() so stale values cannot touch a destroyed state.
    std::shared_ptr<bool> alive = std::make_shared<bool>(true);
};

LuaBridge3Backend::LuaBridge3Backend() : pImpl_(std::make_unique<Impl>()) {
    pImpl_->L = luaL_newstate();
    if (!pImpl_->L) {
        throw std::runtime_error("Failed to create Lua state");
    }
}

LuaBridge3Backend::~LuaBridge3Backend() {
    if (pImpl_->L) {
        // Release all compiled refs
        for (auto& [id, ref] : pImpl_->compiled_) {
            luaL_unref(pImpl_->L, LUA_REGISTRYINDEX, ref);
        }
        pImpl_->compiled_.clear();
        // Announce the state's death before closing it.
        *pImpl_->alive = false;
        lua_close(pImpl_->L);
        pImpl_->L = nullptr;
    }
}

std::shared_ptr<const bool> LuaBridge3Backend::aliveFlag() const {
    return pImpl_->alive;
}

void LuaBridge3Backend::openLibraries() {
    luaL_openlibs(pImpl_->L);

    // Store backend pointer in the registry for closures to access action
    // handlers. The registry is not reachable from Lua code.
    lua_pushlightuserdata(pImpl_->L, const_cast<char*>(&kBackendPtrKey));
    lua_pushlightuserdata(pImpl_->L, this);
    lua_rawset(pImpl_->L, LUA_REGISTRYINDEX);
}

// Fetch the backend pointer stashed by openLibraries(). Returns nullptr if
// absent. Leaves the Lua stack balanced.
static LuaBridge3Backend* backendFromRegistry(lua_State* L) {
    lua_pushlightuserdata(L, const_cast<char*>(&kBackendPtrKey));
    lua_rawget(L, LUA_REGISTRYINDEX);
    auto* backend = static_cast<LuaBridge3Backend*>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    return backend;
}

lua_State* LuaBridge3Backend::state() const {
    return pImpl_->L;
}

void* LuaBridge3Backend::nativeState() const {
    // LuaBridge3 doesn't expose a native state wrapper like sol::state
    return nullptr;
}

void LuaBridge3Backend::reset() {
    // Release all compiled refs
    for (auto& [id, ref] : pImpl_->compiled_) {
        luaL_unref(pImpl_->L, LUA_REGISTRYINDEX, ref);
    }
    pImpl_->compiled_.clear();

    // Release all coroutine thread refs
    for (auto& [threadRef, thread] : pImpl_->coroutines_) {
        if (thread) {
            luaL_unref(pImpl_->L, LUA_REGISTRYINDEX, threadRef);
        }
    }
    pImpl_->coroutines_.clear();

    pImpl_->globals_.clear();
    pImpl_->nativeFuncs_.clear();
    pImpl_->predicates_.clear();
    // Action handler closures live in the old Lua state; clear the backing store
    // too so handler IDs restart from 0 and stale handlers are not retained.
    pImpl_->actionHandlers_.clear();
    pImpl_->actionHandlerIds_.clear();
    // Metatables referencing these are destroyed with the old state.
    pImpl_->boundMethods_.clear();

    // Retire the old liveness flag so any LuaValue still referencing the state
    // we are about to close becomes inert, then install a fresh flag for the
    // new state.
    *pImpl_->alive = false;
    pImpl_->alive = std::make_shared<bool>(true);

    lua_close(pImpl_->L);
    pImpl_->L = luaL_newstate();
    if (!pImpl_->L) {
        throw std::runtime_error("Failed to create replacement Lua state during reset");
    }
    openLibraries();
}

void LuaBridge3Backend::collectGarbage() {
    lua_gc(pImpl_->L, LUA_GCCOLLECT, 0);
}

size_t LuaBridge3Backend::memoryUsageKB() const {
    return static_cast<size_t>(lua_gc(pImpl_->L, LUA_GCCOUNT, 0));
}

void LuaBridge3Backend::compileExpression(const std::string& id, const std::string& code) {
    std::string wrapped = "return " + code;
    int status = luaL_loadstring(pImpl_->L, wrapped.c_str());
    if (status != LUA_OK) {
        const char* msg = lua_tostring(pImpl_->L, -1);
        std::string error = msg ? msg : "Unknown compilation error";
        lua_pop(pImpl_->L, 1);
        throw std::runtime_error("Failed to compile expression: " + error);
    }
    // Store compiled chunk in registry
    int ref = luaL_ref(pImpl_->L, LUA_REGISTRYINDEX);
    // Remove old ref if exists
    auto it = pImpl_->compiled_.find(id);
    if (it != pImpl_->compiled_.end()) {
        luaL_unref(pImpl_->L, LUA_REGISTRYINDEX, it->second);
    }
    pImpl_->compiled_[id] = ref;
}

void LuaBridge3Backend::compileAction(const std::string& id, const std::string& code) {
    int status = luaL_loadstring(pImpl_->L, code.c_str());
    if (status != LUA_OK) {
        const char* msg = lua_tostring(pImpl_->L, -1);
        std::string error = msg ? msg : "Unknown compilation error";
        lua_pop(pImpl_->L, 1);
        throw std::runtime_error("Failed to compile action: " + error);
    }
    // Store compiled chunk in registry
    int ref = luaL_ref(pImpl_->L, LUA_REGISTRYINDEX);
    auto it = pImpl_->compiled_.find(id);
    if (it != pImpl_->compiled_.end()) {
        luaL_unref(pImpl_->L, LUA_REGISTRYINDEX, it->second);
    }
    pImpl_->compiled_[id] = ref;
}

std::unique_ptr<LuaValue> LuaBridge3Backend::evaluate(
    const std::string& id,
    const std::vector<std::pair<std::string, std::any>>& params) {

    auto it = pImpl_->compiled_.find(id);
    if (it == pImpl_->compiled_.end()) {
        throw std::runtime_error("Expression not compiled: " + id);
    }

    // Set parameters as globals
    for (const auto& [name, value] : params) {
        pushAny(pImpl_->L, value);
        lua_setglobal(pImpl_->L, name.c_str());
    }

    // Push compiled function onto stack
    lua_rawgeti(pImpl_->L, LUA_REGISTRYINDEX, it->second);

    // Call it
    int status = lua_pcall(pImpl_->L, 0, 1, 0);

    // Clear parameter globals
    for (const auto& [name, _] : params) {
        lua_pushnil(pImpl_->L);
        lua_setglobal(pImpl_->L, name.c_str());
    }

    if (status != LUA_OK) {
        const char* msg = lua_tostring(pImpl_->L, -1);
        std::string error = msg ? msg : "Unknown error";
        lua_pop(pImpl_->L, 1);
        throw std::runtime_error("Expression evaluation failed: " + error);
    }

    auto result = std::make_unique<LuaBridgeValue>(pImpl_->L, -1, pImpl_->alive);
    lua_pop(pImpl_->L, 1); // pop result
    return result;
}

void LuaBridge3Backend::executeAction(
    const std::string& id,
    const std::vector<std::pair<std::string, std::any>>& params) {

    auto it = pImpl_->compiled_.find(id);
    if (it == pImpl_->compiled_.end()) {
        throw std::runtime_error("Action not compiled: " + id);
    }

    // Set parameters as globals
    for (const auto& [name, value] : params) {
        pushAny(pImpl_->L, value);
        lua_setglobal(pImpl_->L, name.c_str());
    }

    // Push compiled function onto stack
    lua_rawgeti(pImpl_->L, LUA_REGISTRYINDEX, it->second);

    // Call it
    int status = lua_pcall(pImpl_->L, 0, 0, 0);

    // Clear parameter globals
    for (const auto& [name, _] : params) {
        lua_pushnil(pImpl_->L);
        lua_setglobal(pImpl_->L, name.c_str());
    }

    if (status != LUA_OK) {
        const char* msg = lua_tostring(pImpl_->L, -1);
        std::string error = msg ? msg : "Unknown error";
        lua_pop(pImpl_->L, 1);
        throw std::runtime_error("Action execution failed: " + error);
    }
}

void* LuaBridge3Backend::createCoroutine(const std::string& id) {
    auto it = pImpl_->compiled_.find(id);
    if (it == pImpl_->compiled_.end()) {
        return nullptr;
    }

    // Create a new thread. lua_newthread pushes the thread onto the main
    // state's stack, so take a registry reference and pop it to avoid leaking
    // a stack reference for every coroutine.
    lua_State* thread = lua_newthread(pImpl_->L);
    if (!thread) {
        return nullptr;
    }
    int threadRef = luaL_ref(pImpl_->L, LUA_REGISTRYINDEX);

    // Push compiled function onto thread stack
    lua_rawgeti(thread, LUA_REGISTRYINDEX, it->second);

    void* handle = reinterpret_cast<void*>(static_cast<intptr_t>(threadRef));
    pImpl_->coroutines_[threadRef] = thread;
    return handle;
}

bool LuaBridge3Backend::resumeCoroutine(void* handle) {
    int threadRef = static_cast<int>(reinterpret_cast<intptr_t>(handle));
    auto it = pImpl_->coroutines_.find(threadRef);
    if (it == pImpl_->coroutines_.end()) {
        return true; // finished
    }

    lua_State* thread = it->second;
    int nresults = 0;
    int status = lua_resume(thread, pImpl_->L, 0, &nresults);

    if (status == LUA_OK || status == LUA_YIELD) {
        // Check if coroutine is actually done
        if (status == LUA_OK) {
            return true; // finished
        }
        return false; // yielded
    } else {
        // Error
        const char* msg = lua_tostring(thread, -1);
        std::string error = msg ? msg : "Coroutine error";
        lua_pop(thread, 1);
        throw std::runtime_error("Coroutine execution failed: " + error);
    }
}

void LuaBridge3Backend::closeCoroutine(void* handle) {
    int threadRef = static_cast<int>(reinterpret_cast<intptr_t>(handle));
    auto it = pImpl_->coroutines_.find(threadRef);
    if (it != pImpl_->coroutines_.end()) {
        luaL_unref(pImpl_->L, LUA_REGISTRYINDEX, threadRef);
        pImpl_->coroutines_.erase(it);
    }
}

void LuaBridge3Backend::removeCompiled(const std::string& id) {
    auto it = pImpl_->compiled_.find(id);
    if (it != pImpl_->compiled_.end()) {
        luaL_unref(pImpl_->L, LUA_REGISTRYINDEX, it->second);
        pImpl_->compiled_.erase(it);
    }
}

void LuaBridge3Backend::setGlobal(const std::string& name, const LuaValue& value) {
    // Handle LuaBridgeValue directly
    if (auto* lbVal = dynamic_cast<const LuaBridgeValue*>(&value)) {
        lbVal->push();
    } else {
        // Handle other LuaValue types by converting them to Lua stack values
        switch (value.type()) {
            case LuaType::Nil:
                lua_pushnil(pImpl_->L);
                break;
            case LuaType::Boolean:
                lua_pushboolean(pImpl_->L, value.toBool() ? 1 : 0);
                break;
            case LuaType::Number:
                lua_pushnumber(pImpl_->L, value.toNumber());
                break;
            case LuaType::Integer:
                lua_pushinteger(pImpl_->L, value.toInteger());
                break;
            case LuaType::String:
                lua_pushstring(pImpl_->L, value.toString().c_str());
                break;
            case LuaType::Table:
            case LuaType::Function:
            case LuaType::Userdata:
                // For complex types, we push nil as a fallback
                // In a full implementation, we'd need to convert the table properly
                lua_pushnil(pImpl_->L);
                break;
            default:
                lua_pushnil(pImpl_->L);
                break;
        }
    }
    lua_setglobal(pImpl_->L, name.c_str());
    pImpl_->globals_.insert(name);
}

std::unique_ptr<LuaValue> LuaBridge3Backend::getGlobal(const std::string& name) {
    lua_getglobal(pImpl_->L, name.c_str());
    auto val = std::make_unique<LuaBridgeValue>(pImpl_->L, -1, pImpl_->alive);
    lua_pop(pImpl_->L, 1);
    return val;
}

void LuaBridge3Backend::clearGlobals() {
    for (const auto& name : pImpl_->globals_) {
        lua_pushnil(pImpl_->L);
        lua_setglobal(pImpl_->L, name.c_str());
    }
    pImpl_->globals_.clear();
}

void LuaBridge3Backend::registerFunction(const std::string& name, LuaNativeFunc func) {
    pImpl_->nativeFuncs_[name] = std::move(func);

    // Store function pointer in registry
    lua_pushstring(pImpl_->L, name.c_str());
    lua_pushlightuserdata(pImpl_->L, &pImpl_->nativeFuncs_[name]);
    lua_settable(pImpl_->L, LUA_REGISTRYINDEX);
}


// Forward declaration of predicate handler
static int luaPredicateCallHandler(lua_State* L);

// Store predicates in a registry that persists for the lifetime of the backend
// We use a map from string name to function, stored in a way that's accessible
// from the C handler

// Global predicate registry - this is a bit of a hack but ensures stability
// Each backend instance will have its own predicates stored in the Impl
// and we pass the name through the closure upvalue

void LuaBridge3Backend::registerPredicate(const std::string& name, LuaPredicateFunc func) {
    // Store the predicate function in the predicates map
    auto [it, inserted] = pImpl_->predicates_.emplace(name, std::move(func));
    if (!inserted) {
        it->second = std::move(func);
    }
    
    // Create a simple Lua C function that will be called
    // We use a light userdata to store the backend instance pointer
    // and look up the predicate by name at call time
    lua_pushstring(pImpl_->L, name.c_str());
    lua_pushlightuserdata(pImpl_->L, this);
    lua_pushcclosure(pImpl_->L, luaPredicateCallHandler, 2);
    
    // Verify we have a function on the stack
    if (!lua_isfunction(pImpl_->L, -1)) {
        lua_pop(pImpl_->L, 1);
        throw std::runtime_error("Failed to create closure for predicate: " + name);
    }
    
    lua_setglobal(pImpl_->L, name.c_str());
    
    // Verify the global was set
    lua_getglobal(pImpl_->L, name.c_str());
    if (!lua_isfunction(pImpl_->L, -1)) {
        lua_pop(pImpl_->L, 1);
        throw std::runtime_error("Failed to set predicate as global: " + name);
    }
    lua_pop(pImpl_->L, 1);
}

// Static helper to call predicates.
//
// luaL_error() does not return - with Lua built as C (the default) it
// longjmps, which skips the destructors of any C++ object still in scope. So
// every non-trivial local is confined to an inner scope, the message is copied
// onto the Lua stack while that scope is still live, and lua_error() is only
// raised afterwards, when there is nothing left to unwind.
static int luaPredicateCallHandler(lua_State* L) {
    // Get the predicate name from upvalue index 1
    const char* name = lua_tostring(L, lua_upvalueindex(1));
    // Get the backend instance from upvalue index 2
    auto* backend = static_cast<LuaBridge3Backend*>(lua_touserdata(L, lua_upvalueindex(2)));

    if (!name || !backend) {
        return luaL_error(L, "Invalid predicate call - missing name or backend");
    }

    auto* predFunc = backend->getPredicate(name);
    if (!predFunc) {
        return luaL_error(L, "Predicate '%s' not found or null in backend map", name);
    }

    bool result = false;
    bool failed = false;
    {
        // Collect arguments
        int nargs = lua_gettop(L);
        std::vector<std::unique_ptr<LuaValue>> args;
        args.reserve(static_cast<size_t>(nargs));
        for (int i = 1; i <= nargs; ++i) {
            args.push_back(std::make_unique<LuaBridgeValue>(L, i, backend->aliveFlag()));
        }

        try {
            result = (*predFunc)(L, args);
        } catch (const std::exception& e) {
            // A C++ exception must not propagate through the Lua C call
            // boundary; convert it to a Lua error instead.
            lua_pushfstring(L, "Predicate '%s' threw: %s", name, e.what());
            failed = true;
        } catch (...) {
            lua_pushfstring(L, "Predicate '%s' threw an unknown exception", name);
            failed = true;
        }
    }  // all C++ objects destroyed here

    if (failed) {
        return lua_error(L);
    }

    lua_pushboolean(L, result);
    return 1;
}

// Helper to get a predicate by name (used by the static handler)
LuaPredicateFunc* LuaBridge3Backend::getPredicate(const std::string& name) {
    auto it = pImpl_->predicates_.find(name);
    if (it != pImpl_->predicates_.end() && it->second) {
        return &it->second;
    }
    return nullptr;
}

std::unique_ptr<LuaValue> LuaBridge3Backend::makeNil() {
    lua_pushnil(pImpl_->L);
    auto val = std::make_unique<LuaBridgeValue>(pImpl_->L, -1, pImpl_->alive);
    lua_pop(pImpl_->L, 1);
    return val;
}

std::unique_ptr<LuaValue> LuaBridge3Backend::makeBool(bool value) {
    lua_pushboolean(pImpl_->L, value);
    auto val = std::make_unique<LuaBridgeValue>(pImpl_->L, -1, pImpl_->alive);
    lua_pop(pImpl_->L, 1);
    return val;
}

std::unique_ptr<LuaValue> LuaBridge3Backend::makeInt(int value) {
    lua_pushinteger(pImpl_->L, static_cast<lua_Integer>(value));
    auto val = std::make_unique<LuaBridgeValue>(pImpl_->L, -1, pImpl_->alive);
    lua_pop(pImpl_->L, 1);
    return val;
}

std::unique_ptr<LuaValue> LuaBridge3Backend::makeDouble(double value) {
    lua_pushnumber(pImpl_->L, value);
    auto val = std::make_unique<LuaBridgeValue>(pImpl_->L, -1, pImpl_->alive);
    lua_pop(pImpl_->L, 1);
    return val;
}

std::unique_ptr<LuaValue> LuaBridge3Backend::makeString(const std::string& value) {
    lua_pushlstring(pImpl_->L, value.c_str(), value.length());
    auto val = std::make_unique<LuaBridgeValue>(pImpl_->L, -1, pImpl_->alive);
    lua_pop(pImpl_->L, 1);
    return val;
}

std::unique_ptr<LuaValue> LuaBridge3Backend::makeString(const char* value) {
    lua_pushstring(pImpl_->L, value);
    auto val = std::make_unique<LuaBridgeValue>(pImpl_->L, -1, pImpl_->alive);
    lua_pop(pImpl_->L, 1);
    return val;
}

std::unique_ptr<LuaValue> LuaBridge3Backend::makePointer(void* ptr) {
    lua_pushlightuserdata(pImpl_->L, ptr);
    auto val = std::make_unique<LuaBridgeValue>(pImpl_->L, -1, pImpl_->alive);
    lua_pop(pImpl_->L, 1);
    return val;
}

std::unique_ptr<LuaValue> LuaBridge3Backend::createTable() {
    lua_newtable(pImpl_->L);
    auto val = std::make_unique<LuaBridgeValue>(pImpl_->L, -1, pImpl_->alive);
    lua_pop(pImpl_->L, 1);
    return val;
}

// ============================================================================
// Type / Action binding -- stubs for now (full implementation later)
// ============================================================================

void LuaBridge3Backend::bindTypes(TypeRegistry* registry) {
    if (!registry) return;

    for (const auto& [typeIndex, desc] : registry->allTypes()) {
        // Create a metatable for this type
        std::string mtName = "__fastrules_mt_" + desc.name;
        luaL_newmetatable(pImpl_->L, mtName.c_str());

        // Store type descriptor pointer as upvalue (in registry)
        // We need to store it somewhere accessible to the closures
        lua_pushstring(pImpl_->L, "__fields");
        lua_newtable(pImpl_->L); // table of fieldName -> {offset, luaType}
        for (const auto& field : desc.fields) {
            lua_pushstring(pImpl_->L, field.name.c_str());
            lua_newtable(pImpl_->L);
            lua_pushstring(pImpl_->L, "offset");
            lua_pushinteger(pImpl_->L, static_cast<lua_Integer>(field.offset));
            lua_settable(pImpl_->L, -3);
            lua_pushstring(pImpl_->L, "type");
            lua_pushstring(pImpl_->L, field.luaType.c_str());
            lua_settable(pImpl_->L, -3);
            lua_settable(pImpl_->L, -3);
        }
        lua_settable(pImpl_->L, -3);

        // __index closure - handles both fields and methods
        lua_pushstring(pImpl_->L, "__index");
        lua_pushcfunction(pImpl_->L, [](lua_State* L) -> int {
            // arg1: userdata (pointer to object pointer -- void**)
            // arg2: key (field or method name)
            void** ud = static_cast<void**>(lua_touserdata(L, 1));
            if (!ud || !*ud) {
                lua_pushnil(L);
                return 1;
            }
            void* obj = *ud;
            const char* key = lua_tostring(L, 2);
            if (!key) {
                lua_pushnil(L);
                return 1;
            }

            // Get the metatable
            if (!lua_getmetatable(L, 1)) {
                lua_pushnil(L);
                return 1;
            }

            // First check if this is a method (stored directly in metatable)
            lua_pushstring(L, key);
            lua_rawget(L, -2); // look up key in metatable
            if (!lua_isnil(L, -1)) {
                // Found a method - leave it on stack and return
                lua_remove(L, -2); // remove metatable, leave method
                return 1;
            }
            lua_pop(L, 1); // pop nil

            // Not a method, check fields
            lua_pushstring(L, "__fields");
            lua_rawget(L, -2); // get fields table
            if (!lua_istable(L, -1)) {
                lua_pop(L, 2);
                lua_pushnil(L);
                return 1;
            }

            lua_pushstring(L, key);
            lua_rawget(L, -2); // get field info table
            if (!lua_istable(L, -1)) {
                lua_pop(L, 3);
                lua_pushnil(L);
                return 1;
            }

            lua_pushstring(L, "offset");
            lua_rawget(L, -2);
            size_t offset = static_cast<size_t>(lua_tointeger(L, -1));
            lua_pop(L, 1);

            lua_pushstring(L, "type");
            lua_rawget(L, -2);
            std::string typeStr = lua_tostring(L, -1) ? lua_tostring(L, -1) : "";
            lua_pop(L, 1);

            // Pop field info + fields table + metatable
            lua_pop(L, 3);

            // Read value at offset (use memcpy for safe type punning)
            char* ptr = static_cast<char*>(obj);
            if (typeStr == "int") {
                int val;
                std::memcpy(&val, ptr + offset, sizeof(int));
                lua_pushinteger(L, val);
            } else if (typeStr == "double") {
                double val;
                std::memcpy(&val, ptr + offset, sizeof(double));
                lua_pushnumber(L, val);
            } else if (typeStr == "bool") {
                bool val;
                std::memcpy(&val, ptr + offset, sizeof(bool));
                lua_pushboolean(L, val);
            } else if (typeStr == "string") {
                const std::string* val = reinterpret_cast<const std::string*>(ptr + offset);
                lua_pushstring(L, val->c_str());
            } else {
                lua_pushnil(L);
            }
            return 1;
        });
        lua_settable(pImpl_->L, -3);

        // __newindex closure
        lua_pushstring(pImpl_->L, "__newindex");
        lua_pushcfunction(pImpl_->L, [](lua_State* L) -> int {
            // arg1: userdata, arg2: field name, arg3: value
            void** ud = static_cast<void**>(lua_touserdata(L, 1));
            if (!ud || !*ud) return 0;
            void* obj = *ud;
            const char* fieldName = lua_tostring(L, 2);
            if (!fieldName) return 0;

            if (!lua_getmetatable(L, 1)) return 0;
            lua_pushstring(L, "__fields");
            lua_rawget(L, -2);
            if (!lua_istable(L, -1)) {
                lua_pop(L, 2);
                return 0;
            }

            lua_pushstring(L, fieldName);
            lua_rawget(L, -2);
            if (!lua_istable(L, -1)) {
                lua_pop(L, 3);
                return 0;
            }

            lua_pushstring(L, "offset");
            lua_rawget(L, -2);
            size_t offset = static_cast<size_t>(lua_tointeger(L, -1));
            lua_pop(L, 1);

            lua_pushstring(L, "type");
            lua_rawget(L, -2);
            std::string typeStr = lua_tostring(L, -1) ? lua_tostring(L, -1) : "";
            lua_pop(L, 1);
            lua_pop(L, 3); // field info + fields + metatable

            char* ptr = static_cast<char*>(obj);
            if (typeStr == "int") {
                int val = static_cast<int>(lua_tointeger(L, 3));
                std::memcpy(ptr + offset, &val, sizeof(int));
            } else if (typeStr == "double") {
                double val = lua_tonumber(L, 3);
                std::memcpy(ptr + offset, &val, sizeof(double));
            } else if (typeStr == "bool") {
                bool val = lua_toboolean(L, 3);
                std::memcpy(ptr + offset, &val, sizeof(bool));
            } else if (typeStr == "string") {
                const char* s = lua_tostring(L, 3);
                std::string* target = reinterpret_cast<std::string*>(ptr + offset);
                target->~basic_string();
                new (target) std::string(s ? s : "");
            }
            return 0;
        });
        lua_settable(pImpl_->L, -3);

        // Add method closures
        for (const auto& method : desc.methods) {
            // Copy the method into backend-owned storage with a stable address.
            // Re-binding an existing entry overwrites it in place so previously
            // installed metatables keep pointing at a live object.
            const std::string methodKey = mtName + "::" + method.name;
            auto& slot = pImpl_->boundMethods_[methodKey];
            if (slot) {
                *slot = method;
            } else {
                slot = std::make_unique<TypeMethod>(method);
            }
            TypeMethod* stableMethod = slot.get();

            // Store the invoker as a light userdata in the metatable
            lua_pushstring(pImpl_->L, ("__method_" + method.name).c_str());
            lua_pushlightuserdata(pImpl_->L, stableMethod);
            lua_settable(pImpl_->L, -3);

            // Add the method as a closure
            lua_pushstring(pImpl_->L, method.name.c_str());
            lua_pushlightuserdata(pImpl_->L, stableMethod);
            lua_pushcclosure(pImpl_->L, [](lua_State* L) -> int {
                // Get method info from upvalue
                TypeMethod* method = static_cast<TypeMethod*>(lua_touserdata(L, lua_upvalueindex(1)));
                if (!method) return 0;
                
                // Get self (first arg) -- userdata stores void**, dereference it
                void** ud = static_cast<void**>(lua_touserdata(L, 1));
                if (!ud || !*ud) return 0;
                void* obj = *ud;
                
                // Call the invoker
                return method->invoker(obj, L);
            }, 1);
            lua_settable(pImpl_->L, -3);
        }

        lua_pop(pImpl_->L, 1); // pop metatable
    }
}

void LuaBridge3Backend::bindActions(ActionCallbacks* callbacks) {
    if (!callbacks) return;
    
    callbacks->forEachHandler([this](const std::string& name, const ActionCallbacks::Handler& handler) {
        // Reuse this name's existing slot if it has one. Appending
        // unconditionally made actionHandlers_ grow by the full handler set on
        // every bindActions() call - and bindActions() runs on each
        // compileAction(), each Workflow::compile() and once per engine clone.
        int handlerId;
        auto existing = pImpl_->actionHandlerIds_.find(name);
        if (existing != pImpl_->actionHandlerIds_.end()) {
            handlerId = existing->second;
            pImpl_->actionHandlers_[static_cast<size_t>(handlerId)] = handler;
        } else {
            handlerId = static_cast<int>(pImpl_->actionHandlers_.size());
            pImpl_->actionHandlers_.push_back(handler);
            pImpl_->actionHandlerIds_[name] = handlerId;
        }

        lua_pushinteger(pImpl_->L, handlerId);
        lua_pushcclosure(pImpl_->L, [](lua_State* L) -> int {
            int hId = static_cast<int>(lua_tointeger(L, lua_upvalueindex(1)));
            auto* backend = backendFromRegistry(L);
            if (!backend) {
                return luaL_error(L, "Action handler not available");
            }
            if (hId < 0 || hId >= static_cast<int>(backend->pImpl_->actionHandlers_.size())) {
                return luaL_error(L, "Invalid action handler ID");
            }

            // See luaPredicateCallHandler: luaL_error/lua_error longjmp past
            // C++ destructors, so every non-trivial local lives in an inner
            // scope and the error is only raised once that scope has exited.
            bool failed = false;
            {
                // Build parameter list from Lua arguments - check number before
                // string because lua_isstring returns true for numbers too
                int nargs = lua_gettop(L);
                std::vector<std::any> args;
                args.reserve(static_cast<size_t>(nargs));
                for (int i = 1; i <= nargs; ++i) {
                    if (lua_isnumber(L, i)) {
                        args.push_back(lua_tonumber(L, i));
                    } else if (lua_isstring(L, i)) {
                        args.push_back(std::string(lua_tostring(L, i)));
                    } else if (lua_isboolean(L, i)) {
                        args.push_back(lua_toboolean(L, i) != 0);
                    } else {
                        args.push_back(std::any{});
                    }
                }

                try {
                    std::any target;
                    backend->pImpl_->actionHandlers_[static_cast<size_t>(hId)](target, args);
                } catch (const std::exception& e) {
                    lua_pushfstring(L, "Action handler error: %s", e.what());
                    failed = true;
                } catch (...) {
                    lua_pushliteral(L, "Action handler error: unknown exception");
                    failed = true;
                }
            }  // all C++ objects destroyed here

            if (failed) {
                return lua_error(L);
            }
            return 0;
        }, 1);
        lua_setglobal(pImpl_->L, name.c_str());
    });
}

void LuaBridge3Backend::setRegisteredTypeGlobal(const std::string& name, const std::type_index& type, const std::any& value, TypeRegistry* registry) {
    if (!value.has_value()) {
        lua_pushnil(pImpl_->L);
        lua_setglobal(pImpl_->L, name.c_str());
        return;
    }

    // Try to get the descriptor for this registered type
    if (registry) {
        auto descOpt = registry->getDescriptor(type);
        if (descOpt.has_value()) {
            const auto& desc = descOpt.value();
            // Use the type-erased extractPointer to get void* from std::any
            void* ptr = desc.extractPointer ? desc.extractPointer(value) : nullptr;
            if (ptr) {
                // Create a userdata with our custom metatable
                // The metatable has __index/__newindex for fields and __method_* for methods
                void* ud = lua_newuserdata(pImpl_->L, sizeof(void*));
                *static_cast<void**>(ud) = ptr;
                // Set the metatable for this userdata
                std::string mtName = "__fastrules_mt_" + desc.name;
                luaL_getmetatable(pImpl_->L, mtName.c_str());
                if (lua_isnil(pImpl_->L, -1)) {
                    lua_pop(pImpl_->L, 1);
                    // Metatable doesn't exist yet -- bindTypes hasn't been called
                    // Create it now
                    luaL_newmetatable(pImpl_->L, mtName.c_str());
                    // Leave it on stack to be set as metatable
                }
                lua_setmetatable(pImpl_->L, -2);
                lua_setglobal(pImpl_->L, name.c_str());
                return;
            }
        }
    }

    // Fallback: try to extract any pointer type and push as light userdata
    try {
        void* ptr = std::any_cast<void*>(value);
        lua_pushlightuserdata(pImpl_->L, ptr);
        lua_setglobal(pImpl_->L, name.c_str());
    } catch (...) {
        lua_pushnil(pImpl_->L);
        lua_setglobal(pImpl_->L, name.c_str());
    }
}

void LuaBridge3Backend::clearRegisteredTypeGlobal(const std::string& name) {
    lua_pushnil(pImpl_->L);
    lua_setglobal(pImpl_->L, name.c_str());
}

} // namespace fastrules
