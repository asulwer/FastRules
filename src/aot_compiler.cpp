/**
 * @file aot_compiler.cpp
 * @brief Ahead-of-Time (AOT) compilation for rules
 * 
 * This file implements AOT compilation for FastRules:
 * - Compile Lua expressions/actions to bytecode at build time
 * - Save/load bytecode bundles to/from files
 * - Hex string encoding for embedded bundles
 * 
 * AOT Benefits:
 * - Faster startup (no compilation at runtime)
 * - Smaller deployment (bytecode vs source)
 * - Validation at build time (catch errors early)
 * - Distribution without source code
 * 
 * Bundle Format:
 * - Magic: "FAOT" (4 bytes)
 * - Version: uint32 (4 bytes)
 * - Workflow ID: length-prefixed string
 * - Rule count: uint32
 * - For each rule:
 *   - Rule name, expression, action (length-prefixed strings)
 *   - hasExpression, hasAction flags (uint32 each)
 *   - Expression bytecode, action bytecode (length-prefixed strings)
 * 
 * Binary Format:
 * - Big-endian uint32 for lengths
 * - Length-prefixed UTF-8 strings
 * - No compression (can be added externally)
 * 
 * Limitations:
 * - Bytecode is Lua version specific
 * - Platform-specific (endianness)
 * - No bytecode verification (trust required)
 */

#include "fastrules/aot_compiler.hpp"
#include "fastrules/lua_engine.hpp"
#include "fastrules/rule.hpp"
#include "fastrules/workflow.hpp"

#include <sstream>
#include <iomanip>
#include <cstring>
#include <stdexcept>

extern "C" {
#include "lua.h"
#include "lauxlib.h"
}

namespace fastrules {

// ============================================================================
// Binary serialization helpers
// ============================================================================

/**
 * @brief Serialize uint32 as 4 big-endian bytes
 * @param value Value to serialize
 * @return 4-byte string containing serialized value
 */
[[nodiscard]] static std::string writeUint32(uint32_t value) {
    std::string result(4, '\0');
    result[0] = static_cast<char>((value >> 24) & 0xFF);
    result[1] = static_cast<char>((value >> 16) & 0xFF);
    result[2] = static_cast<char>((value >> 8) & 0xFF);
    result[3] = static_cast<char>(value & 0xFF);
    return result;
}

/**
 * @brief Deserialize uint32 from big-endian bytes
 * @param data Binary data
 * @param offset Position to read from
 * @return Deserialized uint32 value
 */
[[nodiscard]] static uint32_t readUint32(const std::string& data, size_t offset) {
    if (offset + 4 > data.size()) return 0;
    return (static_cast<uint32_t>(static_cast<unsigned char>(data[offset])) << 24) |
           (static_cast<uint32_t>(static_cast<unsigned char>(data[offset + 1])) << 16) |
           (static_cast<uint32_t>(static_cast<unsigned char>(data[offset + 2])) << 8) |
           static_cast<uint32_t>(static_cast<unsigned char>(data[offset + 3]));
}

/**
 * @brief Serialize string with length prefix
 * @param str String to serialize
 * @return Length-prefixed string (4-byte length + data)
 */
std::string AotBundle::serializeString(const std::string& str) {
    return writeUint32(static_cast<uint32_t>(str.size())) + str;
}

/**
 * @brief Deserialize length-prefixed string
 * @param data Binary data
 * @param offset Position to read from
 * @return Pair of (string, new_offset)
 */
std::pair<std::string, size_t> AotBundle::deserializeString(const std::string& data, size_t offset) {
    uint32_t len = readUint32(data, offset);
    offset += 4;
    if (offset + len > data.size()) return {{}, data.size()};
    return {data.substr(offset, len), offset + len};
}

// ============================================================================
// AotBundle serialization
// ============================================================================

/**
 * @brief Save bundle to binary file
 * @param path File path to save to
 * @return true if successful
 */
bool AotBundle::saveToFile(const std::string& path) const {
    std::ofstream file(path, std::ios::binary);
    if (!file) return false;
    
    // Header: "FAOT" magic + version
    file.write("FAOT", 4);
    file.write(writeUint32(1).data(), 4);  // version 1
    
    // Workflow ID
    auto idData = serializeString(workflowId);
    file.write(idData.data(), static_cast<std::streamsize>(idData.size()));
    
    // Rule count
    file.write(writeUint32(static_cast<uint32_t>(rules.size())).data(), 4);
    
    for (const auto& rule : rules) {
        auto rid = serializeString(rule.ruleName);
        auto expr = serializeString(rule.expression);
        auto act = serializeString(rule.action);
        auto exprBc = serializeString(rule.expressionBytecode);
        auto actBc = serializeString(rule.actionBytecode);
        
        file.write(rid.data(), static_cast<std::streamsize>(rid.size()));
        file.write(expr.data(), static_cast<std::streamsize>(expr.size()));
        file.write(act.data(), static_cast<std::streamsize>(act.size()));
        file.write(writeUint32(rule.hasExpression ? 1 : 0).data(), 4);
        file.write(writeUint32(rule.hasAction ? 1 : 0).data(), 4);
        file.write(exprBc.data(), static_cast<std::streamsize>(exprBc.size()));
        file.write(actBc.data(), static_cast<std::streamsize>(actBc.size()));
    }
    
    return true;
}

/**
 * @brief Load bundle from binary file
 * @param path File path to load from
 * @return Optional containing bundle if successful
 */
std::optional<AotBundle> AotBundle::loadFromFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) return std::nullopt;
    
    auto size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    std::string data(static_cast<size_t>(size), '\0');
    if (!file.read(data.data(), size)) return std::nullopt;
    
    // Check magic
    if (data.size() < 8 || data.substr(0, 4) != "FAOT") return std::nullopt;
    
    uint32_t version = readUint32(data, 4);
    if (version != 1) return std::nullopt;
    
    size_t offset = 8;
    AotBundle bundle;
    
    auto [wid, off1] = deserializeString(data, offset);
    bundle.workflowId = wid;
    offset = off1;
    
    uint32_t ruleCount = readUint32(data, offset);
    offset += 4;
    
    for (uint32_t i = 0; i < ruleCount; ++i) {
        CompiledRule rule;
        
        auto [rid, off2] = deserializeString(data, offset);
        rule.ruleName = rid;
        offset = off2;
        
        auto [expr, off3] = deserializeString(data, offset);
        rule.expression = expr;
        offset = off3;
        
        auto [act, off4] = deserializeString(data, offset);
        rule.action = act;
        offset = off4;
        
        rule.hasExpression = readUint32(data, offset) != 0;
        offset += 4;
        
        rule.hasAction = readUint32(data, offset) != 0;
        offset += 4;
        
        auto [exprBc, off5] = deserializeString(data, offset);
        rule.expressionBytecode = exprBc;
        offset = off5;
        
        auto [actBc, off6] = deserializeString(data, offset);
        rule.actionBytecode = actBc;
        offset = off6;
        
        bundle.rules.push_back(rule);
    }
    
    return bundle;
}

/**
 * @brief Convert bundle to hex string
 * 
 * Useful for embedding bundles in source code
 * or transmitting over text-only channels.
 * 
 * @return Hex-encoded string
 */
std::string AotBundle::toHexString() const {
    std::ostringstream oss(std::ios::binary);
    
    oss.write("FAOT", 4);
    auto ver = writeUint32(1);
    oss.write(ver.data(), 4);
    
    auto idData = serializeString(workflowId);
    oss.write(idData.data(), static_cast<std::streamsize>(idData.size()));
    
    auto count = writeUint32(static_cast<uint32_t>(rules.size()));
    oss.write(count.data(), 4);
    
    for (const auto& rule : rules) {
        auto rid = serializeString(rule.ruleName);
        auto expr = serializeString(rule.expression);
        auto act = serializeString(rule.action);
        auto exprBc = serializeString(rule.expressionBytecode);
        auto actBc = serializeString(rule.actionBytecode);
        
        oss.write(rid.data(), static_cast<std::streamsize>(rid.size()));
        oss.write(expr.data(), static_cast<std::streamsize>(expr.size()));
        oss.write(act.data(), static_cast<std::streamsize>(act.size()));
        oss.write(writeUint32(rule.hasExpression ? 1 : 0).data(), 4);
        oss.write(writeUint32(rule.hasAction ? 1 : 0).data(), 4);
        oss.write(exprBc.data(), static_cast<std::streamsize>(exprBc.size()));
        oss.write(actBc.data(), static_cast<std::streamsize>(actBc.size()));
    }
    
    std::string binary = oss.str();
    
    std::ostringstream hex;
    hex << std::hex << std::setfill('0');
    for (unsigned char c : binary) {
        hex << std::setw(2) << static_cast<int>(c);
    }
    return hex.str();
}

/**
 * @brief Parse bundle from hex string
 * @param hex Hex-encoded string
 * @return Optional containing bundle if parsing successful
 */
std::optional<AotBundle> AotBundle::fromHexString(const std::string& hex) {
    if (hex.size() % 2 != 0) return std::nullopt;
    
    std::string binary;
    binary.reserve(hex.size() / 2);
    
    for (size_t i = 0; i < hex.size(); i += 2) {
        auto byte = std::strtol(hex.substr(i, 2).c_str(), nullptr, 16);
        binary.push_back(static_cast<char>(byte));
    }
    
    if (binary.size() < 8 || binary.substr(0, 4) != "FAOT") return std::nullopt;
    
    uint32_t version = readUint32(binary, 4);
    if (version != 1) return std::nullopt;
    
    size_t offset = 8;
    AotBundle bundle;
    
    auto [wid, off1] = deserializeString(binary, offset);
    bundle.workflowId = wid;
    offset = off1;
    
    uint32_t ruleCount = readUint32(binary, offset);
    offset += 4;
    
    for (uint32_t i = 0; i < ruleCount; ++i) {
        CompiledRule rule;
        
        auto [rid, off2] = deserializeString(binary, offset);
        rule.ruleName = rid;
        offset = off2;
        
        auto [expr, off3] = deserializeString(binary, offset);
        rule.expression = expr;
        offset = off3;
        
        auto [act, off4] = deserializeString(binary, offset);
        rule.action = act;
        offset = off4;
        
        rule.hasExpression = readUint32(binary, offset) != 0;
        offset += 4;
        
        rule.hasAction = readUint32(binary, offset) != 0;
        offset += 4;
        
        auto [exprBc, off5] = deserializeString(binary, offset);
        rule.expressionBytecode = exprBc;
        offset = off5;
        
        auto [actBc, off6] = deserializeString(binary, offset);
        rule.actionBytecode = actBc;
        offset = off6;
        
        bundle.rules.push_back(rule);
    }
    
    return bundle;
}

// ============================================================================
// AotCompiler implementation
// ============================================================================

/**
 * @brief Compile entire workflow to AOT bundle
 * 
 * Compiles all rules in the workflow, producing a bundle
 * that can be saved and loaded later without recompilation.
 * 
 * @param workflow Workflow to compile
 * @param engine LuaEngine for compilation
 * @return AotBundle containing compiled rules
 */
AotBundle AotCompiler::compileWorkflow(const Workflow& workflow, LuaEngine& engine) {
    AotBundle bundle;
    bundle.workflowId = std::to_string(workflow.id);
    
    for (const auto& rule : workflow.rules) {
        bundle.rules.push_back(compileRule(*rule, engine));
    }
    
    return bundle;
}

/**
 * @brief Compile single rule to AOT format
 * 
 * Compiles expression and action to bytecode using
 * Lua's string.dump functionality.
 * 
 * @param rule Rule to compile
 * @param engine LuaEngine for compilation
 * @return CompiledRule with bytecode
 */
AotBundle::CompiledRule AotCompiler::compileRule(const Rule& rule, LuaEngine& engine) {
    AotBundle::CompiledRule compiled;
    compiled.ruleName = std::to_string(rule.id);
    compiled.expression = rule.expression;
    compiled.action = rule.action;
    
    if (!rule.expression.empty()) {
        auto bc = dumpBytecode(engine, rule.expression);
        if (bc.has_value()) {
            compiled.expressionBytecode = bc.value();
            compiled.hasExpression = true;
        }
    }
    
    if (!rule.action.empty()) {
        auto bc = dumpBytecode(engine, rule.action);
        if (bc.has_value()) {
            compiled.actionBytecode = bc.value();
            compiled.hasAction = true;
        }
    }
    
    return compiled;
}

/**
 * @brief Load compiled bytecode into engine
 * 
 * Loads expression and action bytecode from bundle into
 * the Lua engine. This replaces JIT compilation at runtime.
 * 
 * @param engine LuaEngine to load into
 * @param bundle Bundle containing bytecode
 * @return true if all bytecode loaded successfully
 */
bool AotCompiler::loadBundle(LuaEngine& engine, const AotBundle& bundle) {
    bool allOk = true;
    
    for (const auto& rule : bundle.rules) {
        if (rule.hasExpression && !rule.expressionBytecode.empty()) {
            if (!loadBytecode(engine, rule.expressionBytecode)) {
                allOk = false;
            }
        }
        if (rule.hasAction && !rule.actionBytecode.empty()) {
            if (!loadBytecode(engine, rule.actionBytecode)) {
                allOk = false;
            }
        }
    }
    
    return allOk;
}

/**
 * @brief Check if bundle file is valid
 * @param path Path to bundle file
 * @return true if file exists and has valid magic/version
 */
bool AotCompiler::isBundleValid(const std::string& path) {
    auto bundle = AotBundle::loadFromFile(path);
    return bundle.has_value();
}

/**
 * @brief Dump Lua source to bytecode
 * 
 * Uses Lua C API to compile source and then call string.dump
 * to get the portable bytecode representation.
 * 
 * @param engine LuaEngine to compile with
 * @param source Lua source code
 * @return Optional containing bytecode string
 */
std::optional<std::string> AotCompiler::dumpBytecode(LuaEngine& engine, const std::string& source) {
    lua_State* L = engine.luaState();
    if (!L) return std::nullopt;
    
    // Get string.dump from Lua
    lua_getglobal(L, "string");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        return std::nullopt;
    }
    lua_getfield(L, -1, "dump");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 2);
        return std::nullopt;
    }
    
    // Compile source to function
    if (luaL_loadstring(L, source.c_str()) != LUA_OK) {
        lua_pop(L, 1); // pop error message
        lua_pop(L, 2); // pop string.dump and string table
        return std::nullopt;
    }
    
    // Call string.dump with the compiled function
    lua_pushvalue(L, -1); // duplicate compiled function
    int err = lua_pcall(L, 1, 1, 0);
    if (err != LUA_OK) {
        lua_pop(L, 1); // pop error message
        lua_pop(L, 2); // pop string.dump and string table
        return std::nullopt;
    }
    
    if (lua_isstring(L, -1)) {
        size_t len;
        const char* data = lua_tolstring(L, -1, &len);
        std::string result(data, len);
        lua_pop(L, 3); // pop result, string.dump, string table
        return result;
    }
    
    lua_pop(L, 3);
    return std::nullopt;
}

/**
 * @brief Load bytecode into Lua state
 * 
 * Uses luaL_loadbuffer to load pre-compiled bytecode.
 * Bytecode must match the Lua version exactly.
 * 
 * @param engine LuaEngine to load into
 * @param bytecode Compiled Lua bytecode
 * @return true if loading successful
 */
bool AotCompiler::loadBytecode(LuaEngine& engine, const std::string& bytecode) {
    lua_State* L = engine.luaState();
    if (!L) return false;
    
    if (luaL_loadbuffer(L, bytecode.c_str(), bytecode.size(), "=bytecode") != LUA_OK) {
        lua_pop(L, 1); // pop error message
        return false;
    }
    lua_pop(L, 1); // pop loaded function
    return true;
}

} // namespace fastrules
