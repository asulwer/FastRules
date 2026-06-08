#pragma once

#include <sol/sol.hpp>
#include <future>
#include <functional>
#include <string>
#include <unordered_map>
#include <memory>

namespace fastrules {

// ============================================================================
// Async I/O Registry — Register C++ async functions that Lua can call
// ============================================================================

// Represents a pending async operation
class AsyncPromise {
public:
    using Resolver = std::function<void(const sol::object&)>;

    AsyncPromise() = default;
    explicit AsyncPromise(Resolver resolver) : resolver_(std::move(resolver)) {}

    void resolve(const sol::object& value) {
        if (resolver_) {
            resolver_(value);
            resolved_ = true;
        }
    }

    void resolve(bool value) {
        resolve(sol::make_object(sol::state_view(luaState_), value));
    }

    void resolve(double value) {
        resolve(sol::make_object(sol::state_view(luaState_), value));
    }

    void resolve(const std::string& value) {
        resolve(sol::make_object(sol::state_view(luaState_), value));
    }

    [[nodiscard]] bool isResolved() const { return resolved_; }

    void setLuaState(lua_State* L) { luaState_ = L; }

private:
    Resolver resolver_;
    bool resolved_ = false;
    lua_State* luaState_ = nullptr;
};

// Registry for async functions
class AsyncRegistry {
public:
    using AsyncHandler = std::function<void(const std::vector<sol::object>&, AsyncPromise&)>;

    void registerFunction(const std::string& name, AsyncHandler handler);
    void bindToLua(sol::state& lua);

    [[nodiscard]] bool hasHandler(const std::string& name) const;
    [[nodiscard]] std::vector<std::string> getHandlerNames() const;

private:
    std::unordered_map<std::string, AsyncHandler> handlers_;
};

} // namespace fastrules
