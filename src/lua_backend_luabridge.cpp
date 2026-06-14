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

namespace fastrules {

// ============================================================================
// LuaBridgeValue — wraps a Lua registry reference as LuaValue
// ============================================================================
class LuaBridgeValue : public LuaValue {
public:
    LuaBridgeValue() : L_(nullptr), ref_(LUA_REFNIL) {}
    explicit LuaBridgeValue(lua_State* L, int stackIndex) : L_(L) {
        if (L_) {
            lua_pushvalue(L_, stackIndex);
            ref_ = luaL_ref(L_, LUA_REGISTRYINDEX);
        } else {
            ref_ = LUA_REFNIL;
        }
    }

    ~LuaBridgeValue() override {
        if (L_ && ref_ != LUA_REFNIL && ref_ != LUA_NOREF) {
            luaL_unref(L_, LUA_REGISTRYINDEX, ref_);
        }
    }

    // Move constructor / assignment
    LuaBridgeValue(LuaBridgeValue&& other) noexcept
        : L_(other.L_), ref_(other.ref_) {
        other.ref_ = LUA_NOREF;
    }

    LuaBridgeValue& operator=(LuaBridgeValue&& other) noexcept {
        if (this != &other) {
            if (L_ && ref_ != LUA_REFNIL && ref_ != LUA_NOREF) {
                luaL_unref(L_, LUA_REGISTRYINDEX, ref_);
            }
            L_ = other.L_;
            ref_ = other.ref_;
            other.ref_ = LUA_NOREF;
        }
        return *this;
    }

    // Disable copy
    LuaBridgeValue(const LuaBridgeValue&) = delete;
    LuaBridgeValue& operator=(const LuaBridgeValue&) = delete;

    void push() const {
        if (!L_ || ref_ == LUA_REFNIL || ref_ == LUA_NOREF) {
            if (L_) lua_pushnil(L_);
        } else {
            lua_rawgeti(L_, LUA_REGISTRYINDEX, ref_);
        }
    }

    [[nodiscard]] LuaType type() const override {
        if (!L_ || ref_ == LUA_REFNIL || ref_ == LUA_NOREF) return LuaType::Nil;
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
        if (!L_ || ref_ == LUA_REFNIL || ref_ == LUA_NOREF) return true;
        push();
        bool result = lua_isnil(L_, -1);
        lua_pop(L_, 1);
        return result;
    }

    [[nodiscard]] bool isString() const override {
        if (!L_ || ref_ == LUA_REFNIL || ref_ == LUA_NOREF) return false;
        push();
        bool result = lua_isstring(L_, -1) != 0;
        lua_pop(L_, 1);
        return result;
    }

    [[nodiscard]] bool isTable() const override {
        if (!L_ || ref_ == LUA_REFNIL || ref_ == LUA_NOREF) return false;
        push();
        bool result = lua_istable(L_, -1);
        lua_pop(L_, 1);
        return result;
    }

    [[nodiscard]] bool toBool() const override {
        if (!L_ || ref_ == LUA_REFNIL || ref_ == LUA_NOREF) return false;
        push();
        bool result = lua_toboolean(L_, -1);
        lua_pop(L_, 1);
        return result;
    }

    [[nodiscard]] double toNumber() const override {
        if (!L_ || ref_ == LUA_REFNIL || ref_ == LUA_NOREF) return 0.0;
        push();
        double result = 0.0;
        if (lua_isnumber(L_, -1)) {
            result = lua_tonumber(L_, -1);
        }
        lua_pop(L_, 1);
        return result;
    }

    [[nodiscard]] int64_t toInteger() const override {
        if (!L_ || ref_ == LUA_REFNIL || ref_ == LUA_NOREF) return 0;
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
        if (!L_ || ref_ == LUA_REFNIL || ref_ == LUA_NOREF) return "";
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
        if (!L_ || ref_ == LUA_REFNIL || ref_ == LUA_NOREF) return 0;
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
        if (!L_ || ref_ == LUA_REFNIL || ref_ == LUA_NOREF) return result;
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
        if (!L_ || ref_ == LUA_REFNIL || ref_ == LUA_NOREF) {
            return std::make_unique<LuaBridgeValue>();
        }
        push();
        if (!lua_istable(L_, -1)) {
            lua_pop(L_, 1);
            return std::make_unique<LuaBridgeValue>();
        }
        lua_getfield(L_, -1, key.c_str());
        auto val = std::make_unique<LuaBridgeValue>(L_, -1);
        lua_pop(L_, 2); // pop value and table
        return val;
    }

    void set(const std::string& key, const LuaValue& value) override {
        if (!L_ || ref_ == LUA_REFNIL || ref_ == LUA_NOREF) return;
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

private:
    lua_State* L_;
    int ref_;
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
    } else if (type == typeid(double)) {
        lua_pushnumber(L, std::any_cast<double>(value));
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
struct LuaBridge3Backend::Impl {
    lua_State* L = nullptr;
    std::unordered_map<std::string, int> compiled_;     // id -> registry ref
    std::unordered_map<void*, lua_State*> coroutines_;  // handle -> thread
    std::unordered_set<std::string> globals_;
    std::unordered_map<std::string, LuaNativeFunc> nativeFuncs_;
    std::unordered_map<std::string, LuaPredicateFunc> predicates_;
    std::vector<ActionCallbacks::Handler> actionHandlers_;
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
        lua_close(pImpl_->L);
        pImpl_->L = nullptr;
    }
}

void LuaBridge3Backend::openLibraries() {
    luaL_openlibs(pImpl_->L);
    
    // Store backend pointer for closures to access action handlers
    LuaBridge3Backend** ptr = reinterpret_cast<LuaBridge3Backend**>(
        lua_newuserdata(pImpl_->L, sizeof(LuaBridge3Backend*)));
    *ptr = this;
    lua_setglobal(pImpl_->L, "__fastrules_backend_ptr");
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
    pImpl_->coroutines_.clear();
    pImpl_->globals_.clear();
    pImpl_->nativeFuncs_.clear();
    pImpl_->predicates_.clear();

    lua_close(pImpl_->L);
    pImpl_->L = luaL_newstate();
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

    auto result = std::make_unique<LuaBridgeValue>(pImpl_->L, -1);
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

    // Create a new thread
    lua_State* thread = lua_newthread(pImpl_->L);
    if (!thread) {
        return nullptr;
    }

    // Push compiled function onto thread stack
    lua_rawgeti(thread, LUA_REGISTRYINDEX, it->second);

    void* handle = thread;
    pImpl_->coroutines_[handle] = thread;
    return handle;
}

bool LuaBridge3Backend::resumeCoroutine(void* handle) {
    auto it = pImpl_->coroutines_.find(handle);
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
    pImpl_->coroutines_.erase(handle);
}

void LuaBridge3Backend::removeCompiled(const std::string& id) {
    auto it = pImpl_->compiled_.find(id);
    if (it != pImpl_->compiled_.end()) {
        luaL_unref(pImpl_->L, LUA_REGISTRYINDEX, it->second);
        pImpl_->compiled_.erase(it);
    }
}

void LuaBridge3Backend::setGlobal(const std::string& name, const LuaValue& value) {
    if (auto* lbVal = dynamic_cast<const LuaBridgeValue*>(&value)) {
        lbVal->push();
    } else {
        lua_pushnil(pImpl_->L);
    }
    lua_setglobal(pImpl_->L, name.c_str());
    pImpl_->globals_.insert(name);
}

std::unique_ptr<LuaValue> LuaBridge3Backend::getGlobal(const std::string& name) {
    lua_getglobal(pImpl_->L, name.c_str());
    auto val = std::make_unique<LuaBridgeValue>(pImpl_->L, -1);
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

    // Store function pointer in registry (light userdata approach)
    // Use a C closure with the function name as upvalue
    lua_pushstring(pImpl_->L, name.c_str());
    lua_pushcclosure(pImpl_->L, [](lua_State* L) -> int {
        // Get the function name from upvalue
        const char* funcName = lua_tostring(L, lua_upvalueindex(1));
        if (!funcName) return 0;

        // We can't easily access the backend here without global state,
        // so we store the native funcs in a Lua table and retrieve them
        lua_getglobal(L, "__fastrules_native_funcs");
        if (lua_istable(L, -1)) {
            lua_getfield(L, -1, funcName);
            if (lua_islightuserdata(L, -1)) {
                auto* funcPtr = reinterpret_cast<LuaNativeFunc*>(lua_touserdata(L, -1));
                lua_pop(L, 2); // pop userdata and table

                // Collect arguments
                int nargs = lua_gettop(L);
                std::vector<std::unique_ptr<LuaValue>> args;
                for (int i = 1; i <= nargs; ++i) {
                    args.push_back(std::make_unique<LuaBridgeValue>(L, i));
                }

                auto results = (*funcPtr)(L, args);
                if (!results.empty()) {
                    if (auto* lbVal = dynamic_cast<LuaBridgeValue*>(results[0].get())) {
                        lbVal->push();
                        return 1;
                    }
                }
                return 0;
            }
            lua_pop(L, 2);
        } else {
            lua_pop(L, 1);
        }
        return 0;
    }, 1);
    lua_setglobal(pImpl_->L, name.c_str());

    // Also store in our internal table for lookup
    lua_getglobal(pImpl_->L, "__fastrules_native_funcs");
    if (lua_isnil(pImpl_->L, -1)) {
        lua_pop(pImpl_->L, 1);
        lua_newtable(pImpl_->L);
        lua_setglobal(pImpl_->L, "__fastrules_native_funcs");
        lua_getglobal(pImpl_->L, "__fastrules_native_funcs");
    }
    lua_pushstring(pImpl_->L, name.c_str());
    lua_pushlightuserdata(pImpl_->L, &pImpl_->nativeFuncs_[name]);
    lua_settable(pImpl_->L, -3);
    lua_pop(pImpl_->L, 1);
}

void LuaBridge3Backend::registerPredicate(const std::string& name, LuaPredicateFunc func) {
    pImpl_->predicates_[name] = std::move(func);

    lua_pushstring(pImpl_->L, name.c_str());
    lua_pushcclosure(pImpl_->L, [](lua_State* L) -> int {
        const char* predName = lua_tostring(L, lua_upvalueindex(1));
        if (!predName) {
            lua_pushboolean(L, false);
            return 1;
        }

        lua_getglobal(L, "__fastrules_predicates");
        if (lua_istable(L, -1)) {
            lua_getfield(L, -1, predName);
            if (lua_islightuserdata(L, -1)) {
                auto* predPtr = reinterpret_cast<LuaPredicateFunc*>(lua_touserdata(L, -1));
                lua_pop(L, 2); // pop userdata and table

                int nargs = lua_gettop(L);
                std::vector<std::unique_ptr<LuaValue>> args;
                for (int i = 1; i <= nargs; ++i) {
                    args.push_back(std::make_unique<LuaBridgeValue>(L, i));
                }

                bool result = (*predPtr)(L, args);
                lua_pushboolean(L, result);
                return 1;
            }
            lua_pop(L, 2);
        } else {
            lua_pop(L, 1);
        }
        lua_pushboolean(L, false);
        return 1;
    }, 1);
    lua_setglobal(pImpl_->L, name.c_str());

    // Store in lookup table
    lua_getglobal(pImpl_->L, "__fastrules_predicates");
    if (lua_isnil(pImpl_->L, -1)) {
        lua_pop(pImpl_->L, 1);
        lua_newtable(pImpl_->L);
        lua_setglobal(pImpl_->L, "__fastrules_predicates");
        lua_getglobal(pImpl_->L, "__fastrules_predicates");
    }
    lua_pushstring(pImpl_->L, name.c_str());
    lua_pushlightuserdata(pImpl_->L, &pImpl_->predicates_[name]);
    lua_settable(pImpl_->L, -3);
    lua_pop(pImpl_->L, 1);
}

std::unique_ptr<LuaValue> LuaBridge3Backend::makeNil() {
    lua_pushnil(pImpl_->L);
    auto val = std::make_unique<LuaBridgeValue>(pImpl_->L, -1);
    lua_pop(pImpl_->L, 1);
    return val;
}

std::unique_ptr<LuaValue> LuaBridge3Backend::makeBool(bool value) {
    lua_pushboolean(pImpl_->L, value);
    auto val = std::make_unique<LuaBridgeValue>(pImpl_->L, -1);
    lua_pop(pImpl_->L, 1);
    return val;
}

std::unique_ptr<LuaValue> LuaBridge3Backend::makeInt(int value) {
    lua_pushinteger(pImpl_->L, static_cast<lua_Integer>(value));
    auto val = std::make_unique<LuaBridgeValue>(pImpl_->L, -1);
    lua_pop(pImpl_->L, 1);
    return val;
}

std::unique_ptr<LuaValue> LuaBridge3Backend::makeDouble(double value) {
    lua_pushnumber(pImpl_->L, value);
    auto val = std::make_unique<LuaBridgeValue>(pImpl_->L, -1);
    lua_pop(pImpl_->L, 1);
    return val;
}

std::unique_ptr<LuaValue> LuaBridge3Backend::makeString(const std::string& value) {
    lua_pushlstring(pImpl_->L, value.c_str(), value.length());
    auto val = std::make_unique<LuaBridgeValue>(pImpl_->L, -1);
    lua_pop(pImpl_->L, 1);
    return val;
}

std::unique_ptr<LuaValue> LuaBridge3Backend::makeString(const char* value) {
    lua_pushstring(pImpl_->L, value);
    auto val = std::make_unique<LuaBridgeValue>(pImpl_->L, -1);
    lua_pop(pImpl_->L, 1);
    return val;
}

std::unique_ptr<LuaValue> LuaBridge3Backend::makePointer(void* ptr) {
    lua_pushlightuserdata(pImpl_->L, ptr);
    auto val = std::make_unique<LuaBridgeValue>(pImpl_->L, -1);
    lua_pop(pImpl_->L, 1);
    return val;
}

std::unique_ptr<LuaValue> LuaBridge3Backend::createTable() {
    lua_newtable(pImpl_->L);
    auto val = std::make_unique<LuaBridgeValue>(pImpl_->L, -1);
    lua_pop(pImpl_->L, 1);
    return val;
}

// ============================================================================
// Type / Action binding — stubs for now (full implementation later)
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
            // arg1: userdata (pointer to object pointer — void**)
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
                std::string val;
                std::memcpy(&val, ptr + offset, sizeof(std::string));
                lua_pushstring(L, val.c_str());
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
                std::string val = lua_tostring(L, 3) ? lua_tostring(L, 3) : "";
                std::memcpy(ptr + offset, &val, sizeof(std::string));
            }
            return 0;
        });
        lua_settable(pImpl_->L, -3);

        // Add method closures
        for (const auto& method : desc.methods) {
            // Store the invoker as a light userdata in the metatable
            lua_pushstring(pImpl_->L, ("__method_" + method.name).c_str());
            lua_pushlightuserdata(pImpl_->L, const_cast<TypeMethod*>(&method));
            lua_settable(pImpl_->L, -3);

            // Add the method as a closure
            lua_pushstring(pImpl_->L, method.name.c_str());
            lua_pushlightuserdata(pImpl_->L, const_cast<TypeMethod*>(&method));
            lua_pushcclosure(pImpl_->L, [](lua_State* L) -> int {
                // Get method info from upvalue
                TypeMethod* method = static_cast<TypeMethod*>(lua_touserdata(L, lua_upvalueindex(1)));
                if (!method) return 0;
                
                // Get self (first arg) — userdata stores void**, dereference it
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
        // Wrap the std::any handler in a C closure
        // Store the handler in a persistent map
        int handlerId = static_cast<int>(pImpl_->actionHandlers_.size());
        pImpl_->actionHandlers_.push_back(handler);

        lua_pushinteger(pImpl_->L, handlerId);
        lua_pushcclosure(pImpl_->L, [](lua_State* L) -> int {
            int hId = static_cast<int>(lua_tointeger(L, lua_upvalueindex(1)));
            // Need access to backend's handler list - this requires a registry lookup
            // For simplicity, store a pointer to the backend in a global
            lua_getglobal(L, "__fastrules_backend_ptr");
            auto* backend = reinterpret_cast<LuaBridge3Backend**>(lua_touserdata(L, -1));
            lua_pop(L, 1);
            if (!backend || !*backend) {
                luaL_error(L, "Action handler not available");
                return 0;
            }
            if (hId < 0 || hId >= static_cast<int>((*backend)->pImpl_->actionHandlers_.size())) {
                luaL_error(L, "Invalid action handler ID");
                return 0;
            }
            
            // Build parameter map from Lua arguments - check number before string
            // because lua_isstring returns true for numbers too
            int nargs = lua_gettop(L);
            std::vector<std::any> args;
            args.reserve(nargs);
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
                (*backend)->pImpl_->actionHandlers_[hId](target, args);
            } catch (const std::exception& e) {
                luaL_error(L, "Action handler error: %s", e.what());
                return 0;
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
                    // Metatable doesn't exist yet — bindTypes hasn't been called
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
