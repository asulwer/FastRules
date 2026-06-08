#pragma once

#include "lua_backend.hpp"
#include <sol/sol.hpp>
#include <unordered_map>
#include <unordered_set>

namespace fastrules {

// ============================================================================
// Sol2Backend — LuaBackend implementation using sol2
// ============================================================================
class Sol2Backend : public LuaBackend {
public:
    Sol2Backend();
    ~Sol2Backend() override;

    // Move operations
    Sol2Backend(Sol2Backend&&) noexcept = default;
    Sol2Backend& operator=(Sol2Backend&&) noexcept = default;

    // Disable copy
    Sol2Backend(const Sol2Backend&) = delete;
    Sol2Backend& operator=(const Sol2Backend&) = delete;

    // Compilation
    void compileExpression(const std::string& id, const std::string& code) override;
    void compileAction(const std::string& id, const std::string& code) override;

    // Execution
    [[nodiscard]] std::unique_ptr<LuaValue> evaluate(
        const std::string& id,
        const std::vector<std::pair<std::string, std::any>>& params) override;
    void executeAction(
        const std::string& id,
        const std::vector<std::pair<std::string, std::any>>& params) override;

    // Coroutines
    [[nodiscard]] void* createCoroutine(const std::string& id) override;
    [[nodiscard]] bool resumeCoroutine(void* handle) override;
    void closeCoroutine(void* handle) override;

    // Reference lifecycle
    void removeCompiled(const std::string& id) override;

    // Globals
    void setGlobal(const std::string& name, const LuaValue& value) override;
    [[nodiscard]] std::unique_ptr<LuaValue> getGlobal(const std::string& name) override;
    void clearGlobals() override;

    // Native function / predicate registry
    void registerFunction(const std::string& name, LuaNativeFunc func) override;
    void registerPredicate(const std::string& name, LuaPredicateFunc func) override;

    // State management
    void openLibraries() override;
    [[nodiscard]] lua_State* state() const override;
    [[nodiscard]] void* nativeState() const override;
    void reset() override;
    void collectGarbage() override;
    [[nodiscard]] size_t memoryUsageKB() const override;

    // Value creation helpers
    [[nodiscard]] std::unique_ptr<LuaValue> makeNil() override;
    [[nodiscard]] std::unique_ptr<LuaValue> makeBool(bool value) override;
    [[nodiscard]] std::unique_ptr<LuaValue> makeInt(int value) override;
    [[nodiscard]] std::unique_ptr<LuaValue> makeDouble(double value) override;
    [[nodiscard]] std::unique_ptr<LuaValue> makeString(const std::string& value) override;
    [[nodiscard]] std::unique_ptr<LuaValue> makeString(const char* value) override;
    [[nodiscard]] std::unique_ptr<LuaValue> makePointer(void* ptr) override;
    [[nodiscard]] std::unique_ptr<LuaValue> createTable() override;

    // Type / Action binding
    void bindTypes(TypeRegistry* registry) override;
    void bindActions(ActionCallbacks* callbacks) override;
    void setRegisteredTypeGlobal(const std::string& name, const std::string& typeName, const std::any& value, TypeRegistry* registry) override;
    void clearRegisteredTypeGlobal(const std::string& name) override;

private:
    sol::state lua_;
    std::unordered_map<std::string, sol::function> compiled_;
    std::unordered_map<void*, sol::coroutine> coroutines_;
    std::unordered_set<std::string> globals_;
    std::unordered_map<std::string, LuaNativeFunc> nativeFuncs_;
    std::unordered_map<std::string, LuaPredicateFunc> predicates_;

    [[nodiscard]] sol::object anyToSol_(const std::any& value);
};

} // namespace fastrules
