#include "fastrules/lua_backend_sol2.hpp"
#include "fastrules/type_registry.hpp"
#include "fastrules/action_callback.hpp"
#include <sol/sol.hpp>
#include <stdexcept>

namespace fastrules {

// ============================================================================
// Sol2Value — wraps a sol::object as LuaValue
// ============================================================================
class Sol2Value : public LuaValue {
public:
    Sol2Value() : lua_(static_cast<lua_State*>(nullptr)) {}  // nil value
    explicit Sol2Value(sol::state_view lua, sol::object obj) : lua_(lua), obj_(obj) {}

    [[nodiscard]] LuaType type() const override {
        if (!obj_.valid() || obj_.is<sol::nil_t>()) return LuaType::Nil;
        if (obj_.is<bool>()) return LuaType::Boolean;
        if (obj_.is<int64_t>()) return LuaType::Integer;
        if (obj_.is<double>()) return LuaType::Number;
        if (obj_.is<std::string>()) return LuaType::String;
        if (obj_.is<sol::table>()) return LuaType::Table;
        if (obj_.is<sol::function>()) return LuaType::Function;
        return LuaType::Userdata;
    }

    [[nodiscard]] bool isNil() const override {
        return !obj_.valid() || obj_.is<sol::nil_t>();
    }

    [[nodiscard]] bool isString() const override {
        return obj_.is<std::string>() || obj_.is<const char*>();
    }

    [[nodiscard]] bool isTable() const override {
        return obj_.is<sol::table>();
    }

    [[nodiscard]] bool toBool() const override {
        if (!obj_.valid()) return false;
        if (obj_.is<bool>()) return obj_.as<bool>();
        return true; // Lua truthiness: everything except nil and false is truthy
    }

    [[nodiscard]] double toNumber() const override {
        if (obj_.is<int64_t>()) return static_cast<double>(obj_.as<int64_t>());
        if (obj_.is<double>()) return obj_.as<double>();
        return 0.0;
    }

    [[nodiscard]] int64_t toInteger() const override {
        if (obj_.is<int64_t>()) return obj_.as<int64_t>();
        if (obj_.is<double>()) return static_cast<int64_t>(obj_.as<double>());
        return 0;
    }

    [[nodiscard]] std::string toString() const override {
        if (!obj_.valid()) return "";
        if (obj_.is<std::string>()) return obj_.as<std::string>();
        if (obj_.is<const char*>()) return std::string(obj_.as<const char*>());
        sol::protected_function tostring = lua_["tostring"];
        if (tostring.valid()) {
            auto result = tostring(obj_);
            if (result.valid()) return result.get<std::string>();
        }
        return "";
    }

    [[nodiscard]] size_t length() const override {
        if (!obj_.valid()) return 0;
        if (obj_.is<sol::table>()) {
            sol::table t = obj_.as<sol::table>();
            return t.size();
        }
        if (obj_.is<std::string>()) return obj_.as<std::string>().length();
        return 0;
    }

    [[nodiscard]] std::vector<std::string> keys() const override {
        std::vector<std::string> result;
        if (!obj_.valid() || !obj_.is<sol::table>()) return result;
        sol::table t = obj_.as<sol::table>();
        for (auto& [key, val] : t) {
            if (key.is<std::string>()) {
                result.push_back(key.as<std::string>());
            }
        }
        return result;
    }

    [[nodiscard]] std::unique_ptr<LuaValue> get(const std::string& key) const override {
        if (!obj_.valid() || !obj_.is<sol::table>()) {
            return std::make_unique<Sol2Value>();
        }
        sol::table t = obj_.as<sol::table>();
        return std::make_unique<Sol2Value>(lua_, t[key]);
    }

    void set(const std::string& key, const LuaValue& value) override {
        if (!obj_.valid() || !obj_.is<sol::table>()) return;
        sol::table t = obj_.as<sol::table>();
        if (auto* sv = dynamic_cast<const Sol2Value*>(&value)) {
            t[key] = sv->obj_;
        }
    }

    sol::object obj() const { return obj_; }

private:
    sol::state_view lua_;
    sol::object obj_;
};

// ============================================================================
// Sol2Backend implementation
// ============================================================================
Sol2Backend::Sol2Backend() = default;
Sol2Backend::~Sol2Backend() = default;

void Sol2Backend::openLibraries() {
    lua_.open_libraries(
        sol::lib::base, sol::lib::math, sol::lib::string, sol::lib::table,
        sol::lib::coroutine, sol::lib::package, sol::lib::os, sol::lib::io,
        sol::lib::debug
    );
}

lua_State* Sol2Backend::state() const {
    return lua_.lua_state();
}

void* Sol2Backend::nativeState() const {
    return const_cast<sol::state*>(&lua_);
}

void Sol2Backend::reset() {
    // Clear all compiled functions first (they hold refs to the old state)
    compiled_.clear();
    coroutines_.clear();
    globals_.clear();
    nativeFuncs_.clear();
    predicates_.clear();
    // Now destroy old state and create new one
    lua_.~state();
    new (&lua_) sol::state();
}

void Sol2Backend::collectGarbage() {
    lua_gc(lua_.lua_state(), LUA_GCCOLLECT, 0);
}

size_t Sol2Backend::memoryUsageKB() const {
    return lua_gc(lua_.lua_state(), LUA_GCCOUNT, 0);
}

sol::object Sol2Backend::anyToSol_(const std::any& value) {
    if (!value.has_value()) return sol::nil;
    auto& type = value.type();
    if (type == typeid(bool)) return sol::make_object(lua_, std::any_cast<bool>(value));
    if (type == typeid(int)) return sol::make_object(lua_, std::any_cast<int>(value));
    if (type == typeid(double)) return sol::make_object(lua_, std::any_cast<double>(value));
    if (type == typeid(std::string)) return sol::make_object(lua_, std::any_cast<std::string>(value));
    if (type == typeid(const char*)) return sol::make_object(lua_, std::string(std::any_cast<const char*>(value)));
    return sol::nil;
}

void Sol2Backend::compileExpression(const std::string& id, const std::string& code) {
    auto result = lua_.load("return " + code, id);
    if (!result.valid()) {
        sol::error err = result;
        throw std::runtime_error(std::string("Failed to compile expression: ") + err.what());
    }
    compiled_[id] = result;
}

void Sol2Backend::compileAction(const std::string& id, const std::string& code) {
    auto result = lua_.load(code, id);
    if (!result.valid()) {
        sol::error err = result;
        throw std::runtime_error(std::string("Failed to compile action: ") + err.what());
    }
    compiled_[id] = result;
}

std::unique_ptr<LuaValue> Sol2Backend::evaluate(
    const std::string& id,
    const std::vector<std::pair<std::string, std::any>>& params) {
    auto it = compiled_.find(id);
    if (it == compiled_.end()) {
        throw std::runtime_error("Expression not compiled: " + id);
    }

    for (const auto& [name, value] : params) {
        lua_[name] = anyToSol_(value);
    }

    auto result = it->second();

    for (const auto& [name, _] : params) {
        lua_[name] = sol::nil;
    }

    if (!result.valid()) {
        sol::error err = result;
        throw std::runtime_error(std::string("Expression evaluation failed: ") + err.what());
    }

    sol::object obj = result.get<sol::object>();
    return std::make_unique<Sol2Value>(lua_, obj);
}

void Sol2Backend::executeAction(
    const std::string& id,
    const std::vector<std::pair<std::string, std::any>>& params) {
    auto it = compiled_.find(id);
    if (it == compiled_.end()) {
        throw std::runtime_error("Action not compiled: " + id);
    }

    for (const auto& [name, value] : params) {
        lua_[name] = anyToSol_(value);
    }

    auto result = it->second();

    for (const auto& [name, _] : params) {
        lua_[name] = sol::nil;
    }

    if (!result.valid()) {
        sol::error err = result;
        throw std::runtime_error(std::string("Action execution failed: ") + err.what());
    }
}

void* Sol2Backend::createCoroutine(const std::string& id) {
    auto it = compiled_.find(id);
    if (it == compiled_.end()) {
        return nullptr;
    }
    sol::thread t = sol::thread::create(lua_);
    sol::state_view threadView = t.state();
    threadView[id] = it->second;
    sol::coroutine threadCo = threadView.load("return " + id);
    if (!threadCo.valid()) {
        return nullptr;
    }

    void* handle = threadCo.lua_state();
    coroutines_[handle] = std::move(threadCo);
    return handle;
}

bool Sol2Backend::resumeCoroutine(void* handle) {
    auto it = coroutines_.find(handle);
    if (it == coroutines_.end()) return true;

    sol::coroutine& co = it->second;
    auto result = co();
    if (!result.valid()) {
        return true;
    }
    lua_State* L = co.lua_state();
    int status = lua_status(L);
    if (status == LUA_OK) {
        return true;
    }
    return false;
}

void Sol2Backend::closeCoroutine(void* handle) {
    coroutines_.erase(handle);
}

void Sol2Backend::removeCompiled(const std::string& id) {
    compiled_.erase(id);
}

void Sol2Backend::setGlobal(const std::string& name, const LuaValue& value) {
    if (auto* sv = dynamic_cast<const Sol2Value*>(&value)) {
        lua_[name] = sv->obj();
    } else {
        lua_[name] = sol::nil;
    }
    globals_.insert(name);
}

std::unique_ptr<LuaValue> Sol2Backend::getGlobal(const std::string& name) {
    return std::make_unique<Sol2Value>(lua_, lua_[name]);
}

void Sol2Backend::clearGlobals() {
    for (const auto& name : globals_) {
        lua_[name] = sol::nil;
    }
    globals_.clear();
}

void Sol2Backend::registerFunction(const std::string& name, LuaNativeFunc func) {
    nativeFuncs_[name] = std::move(func);
    lua_[name] = [this, name](sol::variadic_args args) -> sol::object {
        auto it = nativeFuncs_.find(name);
        if (it == nativeFuncs_.end()) return sol::nil;

        std::vector<std::unique_ptr<LuaValue>> vec;
        for (const auto& arg : args) {
            vec.push_back(std::make_unique<Sol2Value>(lua_, arg));
        }

        auto results = it->second(lua_.lua_state(), vec);
        if (results.empty()) return sol::nil;
        if (auto* sv = dynamic_cast<Sol2Value*>(results[0].get())) {
            return sv->obj();
        }
        return sol::nil;
    };
}

void Sol2Backend::registerPredicate(const std::string& name, LuaPredicateFunc func) {
    predicates_[name] = std::move(func);
    lua_[name] = [this, name](sol::variadic_args args) -> bool {
        auto it = predicates_.find(name);
        if (it == predicates_.end()) return false;

        std::vector<std::unique_ptr<LuaValue>> vec;
        for (const auto& arg : args) {
            vec.push_back(std::make_unique<Sol2Value>(lua_, arg));
        }

        return it->second(lua_.lua_state(), vec);
    };
}

std::unique_ptr<LuaValue> Sol2Backend::makeNil() {
    return std::make_unique<Sol2Value>(lua_, sol::make_object(lua_, sol::nil));
}

std::unique_ptr<LuaValue> Sol2Backend::makeBool(bool value) {
    return std::make_unique<Sol2Value>(lua_, sol::make_object(lua_, value));
}

std::unique_ptr<LuaValue> Sol2Backend::makeInt(int value) {
    return std::make_unique<Sol2Value>(lua_, sol::make_object(lua_, static_cast<int64_t>(value)));
}

std::unique_ptr<LuaValue> Sol2Backend::makeDouble(double value) {
    return std::make_unique<Sol2Value>(lua_, sol::make_object(lua_, value));
}

std::unique_ptr<LuaValue> Sol2Backend::makeString(const std::string& value) {
    return std::make_unique<Sol2Value>(lua_, sol::make_object(lua_, value));
}

std::unique_ptr<LuaValue> Sol2Backend::makeString(const char* value) {
    return std::make_unique<Sol2Value>(lua_, sol::make_object(lua_, std::string(value)));
}

std::unique_ptr<LuaValue> Sol2Backend::makePointer(void* ptr) {
    // Push as light userdata — sol2 will wrap it, TypeRegistry will set metatable
    lua_pushlightuserdata(lua_, ptr);
    sol::object obj = sol::stack_object(lua_, -1);
    lua_pop(lua_, 1);
    return std::make_unique<Sol2Value>(lua_, obj);
}

std::unique_ptr<LuaValue> Sol2Backend::createTable() {
    sol::table t = lua_.create_table();
    return std::make_unique<Sol2Value>(lua_, t);
}

// ============================================================================
// Type / Action binding — consume backend-neutral descriptors, produce sol2 bindings
// ============================================================================

void Sol2Backend::bindTypes(TypeRegistry* registry) {
    if (!registry) return;

    for (const auto& [typeIndex, desc] : registry->allTypes()) {
        // Create sol2 usertype with fields
        // Note: sol2 usertype creation is complex; for now we register a simple
        // table-based proxy. Full usertype support requires compile-time type info.
        // TODO: Implement full sol2 usertype binding from TypeDescriptor
        (void)desc; // unused for now — stub
    }
}

void Sol2Backend::bindActions(ActionCallbacks* callbacks) {
    if (!callbacks) return;

    // Bind each handler as a Lua function that converts args to std::any
    callbacks->forEachHandler([this](const std::string& name, const ActionCallbacks::Handler& handler) {
        lua_.set_function(name, [handler](sol::object target, sol::variadic_args va) {
            std::vector<std::any> args;
            for (auto v : va) {
                // Convert sol::object to std::any
                if (v.is<int>()) args.push_back(std::any(v.as<int>()));
                else if (v.is<double>()) args.push_back(std::any(v.as<double>()));
                else if (v.is<bool>()) args.push_back(std::any(v.as<bool>()));
                else if (v.is<std::string>()) args.push_back(std::any(v.as<std::string>()));
                else args.push_back(std::any());
            }
            std::any targetAny;
            if (target.is<int>()) targetAny = target.as<int>();
            else if (target.is<double>()) targetAny = target.as<double>();
            else if (target.is<bool>()) targetAny = target.as<bool>();
            else if (target.is<std::string>()) targetAny = target.as<std::string>();
            handler(targetAny, args);
        });
    });
}

void Sol2Backend::setRegisteredTypeGlobal(const std::string& name, const std::string& /*typeName*/, const std::any& value, TypeRegistry* registry) {
    if (!registry || !value.has_value()) {
        lua_[name] = sol::nil;
        return;
    }
    auto desc = registry->getDescriptor(value.type());
    if (!desc) {
        lua_[name] = sol::nil;
        return;
    }

    // Push as light userdata (full usertype binding requires more work)
    try {
        void* ptr = std::any_cast<void*>(value);
        lua_pushlightuserdata(lua_, ptr);
        sol::object obj = sol::stack_object(lua_, -1);
        lua_pop(lua_, 1);
        lua_[name] = obj;
    } catch (...) {
        lua_[name] = sol::nil;
    }
}

void Sol2Backend::clearRegisteredTypeGlobal(const std::string& name) {
    lua_[name] = sol::nil;
}

} // namespace fastrules

