#pragma once

#include <string>
#include <vector>
#include <any>
#include <functional>
#include <unordered_map>

namespace fastrules {

// ============================================================================
// Action Callback System -- backend-neutral
//
// Handlers receive std::any instead of sol::object. Backends convert
// between their native Lua values and std::any as needed.
// ============================================================================

class ActionCallbacks {
public:
    using Handler = std::function<void(const std::any& target, const std::vector<std::any>& args)>;

    ActionCallbacks() = default;

    void registerHandler(const std::string& name, Handler handler) {
        handlers_[name] = std::move(handler);
    }

    [[nodiscard]] bool hasHandler(const std::string& name) const {
        return handlers_.contains(name);
    }

    void execute(const std::string& name, const std::any& target, const std::vector<std::any>& args) {
        auto it = handlers_.find(name);
        if (it != handlers_.end()) {
            it->second(target, args);
        }
    }

    void registerStub(const std::string& name) {
        if (!hasHandler(name)) {
            registerHandler(name, [](const std::any&, const std::vector<std::any>&) {
                // Stub: does nothing, prevents Lua error on unregistered callback
            });
        }
    }

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

    // Allow backends to iterate handlers for binding
    void forEachHandler(const std::function<void(const std::string&, const Handler&)>& fn) const {
        for (const auto& [name, handler] : handlers_) {
            fn(name, handler);
        }
    }

private:
    std::unordered_map<std::string, Handler> handlers_;
};

} // namespace fastrules
