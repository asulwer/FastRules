#pragma once

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <unordered_map>
#include <fstream>

namespace fastrules {

// Forward declarations
class LuaEngine;
class Rule;
class Workflow;

// Pre-compiled bytecode bundle for AOT distribution
// At build time: compile rules → save bytecode to .dat file
// At runtime: load bytecode → skip compilation overhead
class AotBundle {
public:
    struct CompiledRule {
        std::string ruleName;
        std::string expression;
        std::string action;
        
        // Lua bytecode (binary blob)
        std::string expressionBytecode;
        std::string actionBytecode;
        
        bool hasExpression = false;
        bool hasAction = false;
    };

    std::string workflowId;
    std::vector<CompiledRule> rules;
    
    // Serialize bundle to binary format
    [[nodiscard]] bool saveToFile(const std::string& path) const;
    
    // Deserialize bundle from binary format
    [[nodiscard]] static std::optional<AotBundle> loadFromFile(const std::string& path);
    
    // Serialize to string (for embedding in source code)
    [[nodiscard]] std::string toHexString() const;
    
    // Deserialize from hex string
    [[nodiscard]] static std::optional<AotBundle> fromHexString(const std::string& hex);
    
private:
    [[nodiscard]] static std::string serializeString(const std::string& str);
    [[nodiscard]] static std::pair<std::string, size_t> deserializeString(const std::string& data, size_t offset);
};

// AOT compiler: pre-compiles rules at build time
class AotCompiler {
public:
    // Compile a workflow into an AotBundle
    [[nodiscard]] static AotBundle compileWorkflow(const Workflow& workflow, LuaEngine& engine);
    
    // Compile a single rule into bytecode
    [[nodiscard]] static AotBundle::CompiledRule compileRule(const Rule& rule, LuaEngine& engine);
    
    // Load a pre-compiled bundle into a LuaEngine
    // Returns true if all rules loaded successfully
    [[nodiscard]] static bool loadBundle(LuaEngine& engine, const AotBundle& bundle);
    
    // Quick check: does a bundle file exist and is valid?
    [[nodiscard]] static bool isBundleValid(const std::string& path);
    
private:
    [[nodiscard]] static std::optional<std::string> dumpBytecode(LuaEngine& engine, const std::string& source);
    [[nodiscard]] static bool loadBytecode(LuaEngine& engine, const std::string& bytecode);
};

} // namespace fastrules
