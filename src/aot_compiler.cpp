#include "fastrules/aot_compiler.hpp"
#include "fastrules/lua_engine.hpp"
#include "fastrules/rule.hpp"
#include "fastrules/workflow.hpp"

#ifdef FASTRULES_USE_SOL2

#include <sol/sol.hpp>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <stdexcept>

namespace fastrules {

// ============================================================================
// Binary serialization helpers
// ============================================================================

[[nodiscard]] static std::string writeUint32(uint32_t value) {
    std::string result(4, '\0');
    result[0] = static_cast<char>((value >> 24) & 0xFF);
    result[1] = static_cast<char>((value >> 16) & 0xFF);
    result[2] = static_cast<char>((value >> 8) & 0xFF);
    result[3] = static_cast<char>(value & 0xFF);
    return result;
}

[[nodiscard]] static uint32_t readUint32(const std::string& data, size_t offset) {
    if (offset + 4 > data.size()) return 0;
    return (static_cast<uint32_t>(static_cast<unsigned char>(data[offset])) << 24) |
           (static_cast<uint32_t>(static_cast<unsigned char>(data[offset + 1])) << 16) |
           (static_cast<uint32_t>(static_cast<unsigned char>(data[offset + 2])) << 8) |
           static_cast<uint32_t>(static_cast<unsigned char>(data[offset + 3]));
}

std::string AotBundle::serializeString(const std::string& str) {
    return writeUint32(static_cast<uint32_t>(str.size())) + str;
}

std::pair<std::string, size_t> AotBundle::deserializeString(const std::string& data, size_t offset) {
    uint32_t len = readUint32(data, offset);
    offset += 4;
    if (offset + len > data.size()) return {{}, data.size()};
    return {data.substr(offset, len), offset + len};
}

// ============================================================================
// AotBundle serialization
// ============================================================================

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
        auto rid = serializeString(rule.ruleId);
        auto expr = serializeString(rule.expression);
        auto act = serializeString(rule.action);
        auto pnames = serializeString(std::string());  // placeholder for parameter names
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
        rule.ruleId = rid;
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

std::string AotBundle::toHexString() const {
    // First serialize to binary, then hex encode
    // Use a temp file approach or memory stream
    std::ostringstream oss(std::ios::binary);
    
    oss.write("FAOT", 4);
    auto ver = writeUint32(1);
    oss.write(ver.data(), 4);
    
    auto idData = serializeString(workflowId);
    oss.write(idData.data(), static_cast<std::streamsize>(idData.size()));
    
    auto count = writeUint32(static_cast<uint32_t>(rules.size()));
    oss.write(count.data(), 4);
    
    for (const auto& rule : rules) {
        auto rid = serializeString(rule.ruleId);
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

std::optional<AotBundle> AotBundle::fromHexString(const std::string& hex) {
    if (hex.size() % 2 != 0) return std::nullopt;
    
    std::string binary;
    binary.reserve(hex.size() / 2);
    
    for (size_t i = 0; i < hex.size(); i += 2) {
        auto byte = std::strtol(hex.substr(i, 2).c_str(), nullptr, 16);
        binary.push_back(static_cast<char>(byte));
    }
    
    // Write to temp and load
    // Simpler: parse directly
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
        rule.ruleId = rid;
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

AotBundle AotCompiler::compileWorkflow(const Workflow& workflow, LuaEngine& engine) {
    AotBundle bundle;
    bundle.workflowId = workflow.id;
    
    for (const auto& rule : workflow.rules) {
        bundle.rules.push_back(compileRule(*rule, engine));
    }
    
    return bundle;
}

AotBundle::CompiledRule AotCompiler::compileRule(const Rule& rule, LuaEngine& engine) {
    AotBundle::CompiledRule compiled;
    compiled.ruleId = rule.id;
    compiled.expression = rule.expression;
    compiled.action = rule.action;
    compiled.parameterNames = rule.parameterNames;
    
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

bool AotCompiler::isBundleValid(const std::string& path) {
    auto bundle = AotBundle::loadFromFile(path);
    return bundle.has_value();
}

std::optional<std::string> AotCompiler::dumpBytecode(LuaEngine& engine, const std::string& source) {
    // Use Lua's string.dump to get bytecode
    sol::state& lua = engine.state();
    
    try {
        // Compile to a function
        sol::load_result loaded = lua.load(source);
        if (!loaded.valid()) {
            return std::nullopt;
        }
        
        sol::function func = loaded;
        if (!func.valid()) {
            return std::nullopt;
        }
        
        // Use string.dump equivalent via Lua
        sol::function stringDump = lua["string"]["dump"];
        if (!stringDump.valid()) {
            // string.dump might be restricted, fallback to storing source
            return std::nullopt;
        }
        
        auto result = stringDump(func);
        if (result.valid() && result.get_type() == sol::type::string) {
            return result.get<std::string>();
        }
        
        return std::nullopt;
    } catch (...) {
        return std::nullopt;
    }
}

bool AotCompiler::loadBytecode(LuaEngine& engine, const std::string& bytecode) {
    sol::state& lua = engine.state();
    
    try {
        // Use loadstring equivalent to load bytecode
        sol::load_result loaded = lua.load(bytecode);
        if (!loaded.valid()) {
            return false;
        }
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace fastrules
#endif // FASTRULES_USE_SOL2
