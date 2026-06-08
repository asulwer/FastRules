#pragma once

#include "lua_backend.hpp"
#include <memory>
#include <unordered_map>

// Forward declarations for lua_State
struct lua_State;

namespace fastrules {

// ============================================================================
// LuaBridge3Backend — LuaBackend implementation using LuaBridge3
// ============================================================================

class LuaBridge3Backend : public LuaBackend {
public:
    LuaBridge3Backend();
    ~LuaBridge3Backend() override;

    // Disable copy, enable move
    LuaBridge3Backend(const LuaBridge3Backend&) = delete;
    LuaBridge3Backend& operator=(const LuaBridge3Backend&) = delete;
    LuaBridge3Backend(LuaBridge3Backend&&) noexcept = default;
    LuaBridge3Backend& operator=(LuaBridge3Backend&&) noexcept = default;

    // ── Compilation ─────────────────────────────────────────────────────────
    void compileExpression(const std::string& id, const std::string& code) override;
    void compileAction(const std::string& id, const std::string& code) override;

    // ── Execution ───────────────────────────────────────────────────────────
    [[nodiscard]] std::unique_ptr<LuaValue> evaluate(
        const std::string& id,
        const std::vector<std::pair<std::string, std::any>>& params) override;

    void executeAction(
        const std::string& id,
        const std::vector<std::pair<std::string, std::any>>& params) override;

    // ── Coroutines ──────────────────────────────────────────────────────────
    [[nodiscard]] void* createCoroutine(const std::string& id) override;
    [[nodiscard]] bool resumeCoroutine(void* handle) override;
    void closeCoroutine(void* handle) override;

    // ── Reference lifecycle ─────────────────────────────────────────────────
    void removeCompiled(const std::string& id) override;

    // ── Globals ─────────────────────────────────────────────────────────────
    void setGlobal(const std::string& name, const LuaValue& value) override;
    [[nodiscard]] std::unique_ptr<LuaValue> getGlobal(const std::string& name) override;
    void clearGlobals() override;

    // ── Native function / predicate registry ────────────────────────────────
    void registerFunction(const std::string& name, LuaNativeFunc func) override;
    void registerPredicate(const std::string& name, LuaPredicateFunc func) override;

    // ── State management ──────────────────────────────────────────────────
    void openLibraries() override;
    [[nodiscard]] lua_State* state() const override;
    [[nodiscard]] void* nativeState() const override;
    void reset() override;
    void collectGarbage() override;
    [[nodiscard]] size_t memoryUsageKB() const override;

    // ── Value creation helpers ──────────────────────────────────────────────
    [[nodiscard]] std::unique_ptr<LuaValue> makeNil() override;
    [[nodiscard]] std::unique_ptr<LuaValue> makeBool(bool value) override;
    [[nodiscard]] std::unique_ptr<LuaValue> makeInt(int value) override;
    [[nodiscard]] std::unique_ptr<LuaValue> makeDouble(double value) override;
    [[nodiscard]] std::unique_ptr<LuaValue> makeString(const std::string& value) override;
    [[nodiscard]] std::unique_ptr<LuaValue> makeString(const char* value) override;
    [[nodiscard]] std::unique_ptr<LuaValue> makePointer(void* ptr) override;

    // ── Table creation ─────────────────────────────────────────────────────
    [[nodiscard]] std::unique_ptr<LuaValue> createTable() override;

    // ── Type / Action binding (not implemented for LuaBridge3 yet) ──────────
    void bindTypes(TypeRegistry* /*registry*/) override {}
    void bindActions(ActionCallbacks* /*callbacks*/) override {}
    void setRegisteredTypeGlobal(const std::string& /*name*/, const std::string& /*typeName*/, const std::any& /*value*/, TypeRegistry* /*registry*/) override {}
    void clearRegisteredTypeGlobal(const std::string& /*name*/) override {}

private:
    struct Impl;
    std::unique_ptr<Impl> pImpl_;
};

} // namespace fastrules
