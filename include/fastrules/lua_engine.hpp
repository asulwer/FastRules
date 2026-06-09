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
#include <spdlog/spdlog.h>
#include "lua_backend.hpp"

namespace fastrules {

// ============================================================================
// Convenience: TypeRegistrar for lambda-based registration
// Usage inside registerType lambda: reg.bind("name", &T::name);
// ============================================================================
template<typename T>
struct TypeRegistrar {
    std::vector<TypeField> fields;

    template<typename MemberT>
    void bind(const std::string& name, MemberT T::*member) {
        std::string luaType;
        if constexpr (std::is_same_v<MemberT, int> || std::is_same_v<MemberT, int32_t> || std::is_same_v<MemberT, int64_t> || std::is_same_v<MemberT, long>) {
            luaType = "int";
        } else if constexpr (std::is_same_v<MemberT, double> || std::is_same_v<MemberT, float>) {
            luaType = "double";
        } else if constexpr (std::is_same_v<MemberT, bool>) {
            luaType = "bool";
        } else if constexpr (std::is_same_v<MemberT, std::string> || std::is_same_v<MemberT, const char*>) {
            luaType = "string";
        } else {
            luaType = "userdata";
        }
        fields.push_back({name, reinterpret_cast<size_t>(&(((T*)nullptr)->*member)), luaType});
    }
};

// ============================================================================
// Lua engine wrapper using pluggable LuaBackend
//
// Fully backend-agnostic. Works with Sol2Backend, LuaBridge3Backend, or any
// future backend that implements the LuaBackend interface.
// ============================================================================
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

    // Raw lua_State access (backend-neutral)
    [[nodiscard]] lua_State* luaState() const noexcept;

    void setLogger(std::shared_ptr<spdlog::logger> logger) { logger_ = std::move(logger); }
    [[nodiscard]] bool hasLogger() const { return logger_ != nullptr; }
    [[nodiscard]] std::shared_ptr<spdlog::logger> getLogger() const { return logger_; }

    void setMaxExpressionLength(size_t maxLength) { maxExpressionLength_ = maxLength; }
    [[nodiscard]] size_t getMaxExpressionLength() const { return maxExpressionLength_; }

    void registerPredicates();
    [[nodiscard]] std::unique_ptr<LuaEngine> clone() const;

    // Type registration — backend-neutral via TypeDescriptor
    template<typename T>
    void registerType(const std::string& name, std::vector<TypeField> fields) {
        typeRegistry_.registerType<T>(name, std::move(fields));
        backend_->bindTypes(&typeRegistry_);
    }

    // Convenience overload: lambda-based registration (compile-time safe)
    // Usage: engine.registerType<Customer>("Customer", [](auto& reg) {
    //     reg.bind("name", &Customer::name);
    //     reg.bind("age", &Customer::age);
    // });
    template<typename T, typename Func>
    void registerType(const std::string& name, Func func) {
        TypeRegistrar<T> registrar;
        func(registrar);
        typeRegistry_.registerType<T>(name, std::move(registrar.fields));
        backend_->bindTypes(&typeRegistry_);
    }

    [[nodiscard]] bool isTypeRegistered(const std::string& name) const {
        return typeRegistry_.isRegistered(name);
    }

    // Action registration — backend-neutral via std::any Handler
    void registerAction(const std::string& name, ActionCallbacks::Handler handler) {
        actionCallbacks_.registerHandler(name, std::move(handler));
        backend_->bindActions(&actionCallbacks_);
    }

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

    std::shared_ptr<spdlog::logger> logger_;

    size_t maxExpressionLength_ = 0;
    std::atomic<size_t> compileCount_{0};
    std::atomic<size_t> generation_{1};
    size_t autoResetThresholdKB_ = 0;

    // Thread-local context for parallel execution
    static thread_local RuleContext* currentContext_;

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
