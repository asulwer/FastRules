#pragma once

#ifdef FASTRULES_USE_SOL2
#include <sol/sol.hpp>
#endif
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>
#include <any>

namespace fastrules {

// ============================================================================
// Action Callback System
// Allows C++ code to register handlers for Lua actions
// ============================================================================

#ifdef FASTRULES_USE_SOL2

class ActionCallbacks {
public:
    using Handler = std::function<void(sol::object target, const std::vector<sol::object>& args)>;

    ActionCallbacks() = default;

    void registerHandler(const std::string& name, Handler handler) {
        handlers_[name] = std::move(handler);
    }

    [[nodiscard]] bool hasHandler(const std::string& name) const {
        return handlers_.contains(name);
    }

    void execute(const std::string& name, sol::object target, const std::vector<sol::object>& args) {
        auto it = handlers_.find(name);
        if (it != handlers_.end()) {
            it->second(target, args);
        }
    }

    void bindToLua(sol::state& lua);
    void registerStub(const std::string& name);

    [[nodiscard]] std::vector<std::string> getHandlerNames() const {
        std::vector<std::string> names;
        for (const auto& [name, _] : handlers_) {
            names.push_back(name);
        }
        return names;
    }

    void clear() {
        handlers_.clear();
    }

private:
    std::unordered_map<std::string, Handler> handlers_;
};

#else  // !FASTRULES_USE_SOL2

// Stub ActionCallbacks for non-sol2 backends
class ActionCallbacks {
public:
    using Handler = std::function<void(void)>;  // dummy signature

    ActionCallbacks() = default;

    void registerHandler(const std::string&, auto) {}
    [[nodiscard]] bool hasHandler(const std::string&) const { return false; }
    void execute(const std::string&, auto, auto) {}
    void bindToLua(auto&) {}
    void registerStub(const std::string&) {}

    [[nodiscard]] std::vector<std::string> getHandlerNames() const { return {}; }
    void clear() {}

private:
    std::unordered_map<std::string, Handler> handlers_;
};

#endif // FASTRULES_USE_SOL2

} // namespace fastrules
