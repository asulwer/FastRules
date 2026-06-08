#pragma once

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <functional>
#include <unordered_map>
#include <any>
#include <shared_mutex>
#include <mutex>
#include <chrono>
#include <atomic>

#include "rule.hpp"
#include "rule_result.hpp"
#include "rule_context.hpp"
#include "type_registry.hpp"
#include "action_callback.hpp"
#include "logger.hpp"
#include "lua_backend.hpp"
#ifdef FASTRULES_USE_SOL2
#include <sol/sol.hpp>
#endif

namespace fastrules {

// Lua engine wrapper using pluggable LuaBackend
class LuaEngine {
public:
    LuaEngine();
    ~LuaEngine();

    // Disable copy, enable move
    LuaEngine(const LuaEngine&) = delete;
    LuaEngine& operator=(const LuaEngine&) = delete;
    LuaEngine(LuaEngine&&) noexcept;
    LuaEngine& operator=(LuaEngine&&) noexcept;

    [[nodiscard]] std::optional<int> compileExpression(
        const std::string& expression);

    [[nodiscard]] std::optional<int> compileAction(
        const std::string& action);

    [[nodiscard]] bool evaluateExpression(
        int ref,
        const std::vector<RuleParameter>& parameters,
        RuleContext& context,
        std::optional<std::chrono::milliseconds> timeout = std::nullopt);

    void executeAction(
        int ref,
        const std::vector<RuleParameter>& parameters,
        RuleContext& context,
        std::optional<std::chrono::milliseconds> timeout = std::nullopt);

    void setGlobal(const std::string& name, const std::any& value);
    void clearGlobals();

    [[nodiscard]] std::optional<int> compileCoroutine(
        const std::string& expression);

    [[nodiscard]] bool resumeCoroutine(
        int ref,
        const std::vector<RuleParameter>& parameters,
        RuleContext& context);

    [[nodiscard]] bool isCoroutine(int ref) const;

    // await returns std::any (not sol::object) for backend neutrality
    [[nodiscard]] std::optional<std::any> await(
        int ref,
        const std::vector<RuleParameter>& parameters,
        RuleContext& context);

    void releaseRef(int ref);

    class RefGuard {
    public:
        explicit RefGuard(LuaEngine& engine, int ref) noexcept
            : engine_(&engine), ref_(ref) {}
        ~RefGuard() {
            if (engine_ && ref_ >= 0) {
                engine_->releaseRef(ref_);
            }
        }
        RefGuard(const RefGuard&) = delete;
        RefGuard& operator=(const RefGuard&) = delete;
        RefGuard(RefGuard&& other) noexcept
            : engine_(other.engine_), ref_(other.ref_) {
            other.engine_ = nullptr;
            other.ref_ = -1;
        }
        RefGuard& operator=(RefGuard&& other) noexcept {
            if (this != &other) {
                if (engine_ && ref_ >= 0) engine_->releaseRef(ref_);
                engine_ = other.engine_;
                ref_ = other.ref_;
                other.engine_ = nullptr;
                other.ref_ = -1;
            }
            return *this;
        }
        [[nodiscard]] int ref() const noexcept { return ref_; }
    private:
        LuaEngine* engine_;
        int ref_;
    };

    // Backward-compat: returns sol::state& if backend is Sol2Backend, else throws
#ifdef FASTRULES_USE_SOL2
    [[nodiscard]] sol::state& state();
    [[nodiscard]] const sol::state& state() const;
#endif
    [[nodiscard]] lua_State* luaState() const noexcept;

    void setLogger(std::shared_ptr<Logger> logger) { logger_ = std::move(logger); }
    [[nodiscard]] bool hasLogger() const { return logger_ != nullptr; }

    void setMaxExpressionLength(size_t maxLength) { maxExpressionLength_ = maxLength; }
    [[nodiscard]] size_t getMaxExpressionLength() const { return maxExpressionLength_; }

    void registerPredicates();
    [[nodiscard]] std::unique_ptr<LuaEngine> clone() const;

#ifdef FASTRULES_USE_SOL2
    template<typename T>
    void registerType(const std::string& name, typename TypeBinder<T>::BinderFunc binder) {
        typeRegistry_.registerType<T>(name, std::move(binder));
        // Bind via native state if available (sol2 path)
        if (void* native = backend_->nativeState(); native != nullptr) {
            typeRegistry_.bindAll(*static_cast<sol::state*>(native));
        }
    }

    [[nodiscard]] bool isTypeRegistered(const std::string& name) const {
        return typeRegistry_.isRegistered(name);
    }

    void registerAction(const std::string& name, ActionCallbacks::Handler handler) {
        actionCallbacks_.registerHandler(name, std::move(handler));
        if (void* native = backend_->nativeState(); native != nullptr) {
            actionCallbacks_.bindToLua(*static_cast<sol::state*>(native));
        }
    }
#else
    template<typename T>
    void registerType(const std::string&, auto) {}

    [[nodiscard]] bool isTypeRegistered(const std::string&) const { return false; }

    void registerAction(const std::string&, auto) {}
#endif

    void discoverCallbacks(const std::vector<std::string>& actions);

    [[nodiscard]] bool hasAction(const std::string& name) const {
        return actionCallbacks_.hasHandler(name);
    }

    [[nodiscard]] std::vector<std::string> getRegisteredActions() const {
        return actionCallbacks_.getHandlerNames();
    }

    void resetState();
    void collectGarbage();
    [[nodiscard]] size_t getMemoryUsageKB();

    void setAutoResetThreshold(size_t kb) { autoResetThresholdKB_ = kb; }
    [[nodiscard]] size_t getAutoResetThreshold() const { return autoResetThresholdKB_; }
    [[nodiscard]] size_t getCompileCount() const { return compileCount_.load(); }
    [[nodiscard]] size_t getGeneration() const { return generation_.load(); }

private:
    std::unique_ptr<LuaBackend> backend_;
    mutable std::shared_mutex registryMutex_;
    std::mutex luaStateMutex_;
    std::unordered_map<int, std::string> refToBackendId_;
    std::unordered_map<int, bool> coroutineRegistry_;
    std::unordered_map<int, void*> coroutineHandles_;  // ref -> opaque handle
    std::unordered_map<int, std::vector<std::string>> paramNames_;
    int nextRefId_ = 1;

    TypeRegistry typeRegistry_;
    ActionCallbacks actionCallbacks_;

    std::shared_ptr<Logger> logger_;

    size_t maxExpressionLength_ = 0;
    std::atomic<size_t> compileCount_{0};
    std::atomic<size_t> generation_{1};
    size_t autoResetThresholdKB_ = 0;

    void setupEnvironment();
    void setupContextTable(RuleContext& context);
    [[nodiscard]] std::string makeBackendId(int ref) const;
    [[nodiscard]] std::string wrapExpression(const std::string& expression);
    [[nodiscard]] std::string wrapAction(const std::string& action);
    [[nodiscard]] std::string wrapCoroutine(const std::string& expression);

    [[nodiscard]] std::vector<std::pair<std::string, std::any>> buildParamPairs(
        int ref,
        const std::vector<RuleParameter>& parameters);

    [[nodiscard]] std::any luaValueToAny(const LuaValue& value) const;
    void bindTypesToState();
    void bindActionsToState();
};

} // namespace fastrules
