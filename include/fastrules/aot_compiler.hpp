/**
 * @file aot_compiler.hpp
 * @brief Ahead-of-Time (AOT) compilation for FastRules
 * 
 * Provides caching and pre-compilation of Lua expressions to improve performance.
 */

#pragma once

#include <string>
#include <unordered_map>
#include <memory>
#include <filesystem>
#include <chrono>

// Forward declarations
struct lua_State;

namespace fastrules {

/**
 * @brief AOT compiled chunk with metadata
 */
struct AotCompiledChunk {
    std::string bytecode;                    ///< Compiled bytecode
    std::chrono::system_clock::time_point compiledAt;  ///< Compilation timestamp
    size_t hitCount = 0;                     ///< Cache hit counter
};

/**
 * @brief Ahead-of-Time compiler for Lua expressions
 * 
 * Caches compiled Lua chunks to avoid repeated compilation overhead.
 * Can pre-compile common expressions at build time.
 */
class AotCompiler {
private:
    std::unordered_map<std::string, AotCompiledChunk> cache_;
    std::filesystem::path cacheDirectory_;
    bool useDiskCache_ = true;

public:
    /**
     * @brief Construct AOT compiler
     * 
     * @param cacheDir Directory for disk cache (optional)
     */
    explicit AotCompiler(const std::filesystem::path& cacheDir = ".fastrules_cache");

    /**
     * @brief Compile expression to bytecode
     * 
     * @param expression Lua expression to compile
     * @param lua Lua state for compilation
     * @return Compiled bytecode
     */
    std::string compileToBytecode(const std::string& expression, lua_State* lua);

    /**
     * @brief Get compiled chunk from cache or compile if needed
     * 
     * @param expression Lua expression
     * @param lua Lua state
     * @return AOT compiled chunk
     */
    const AotCompiledChunk& getOrCompile(const std::string& expression, lua_State* lua);

    /**
     * @brief Pre-compile expressions at build time
     * 
     * @param expressions List of expressions to pre-compile
     * @param lua Lua state
     */
    void precompile(const std::vector<std::string>& expressions, lua_State* lua);

    /**
     * @brief Save cache to disk
     */
    void saveCache();

    /**
     * @brief Load cache from disk
     */
    void loadCache();

    /**
     * @brief Clear cache
     */
    void clearCache();

    /**
     * @brief Get cache statistics
     * 
     * @return Cache hit ratio and size
     */
    std::pair<double, size_t> getCacheStats() const;
};

/**
 * @brief Simple Lua expression cache
 * 
 * Lightweight cache for compiled Lua expressions.
 */
class LuaExpressionCache {
private:
    std::unordered_map<std::string, std::string> cache_;
    size_t maxCacheSize_ = 1000;

public:
    /**
     * @brief Get or compile expression
     * 
     * @param expression Lua expression
     * @param compileFunc Function to compile expression if not cached
     * @return Compiled expression
     */
    std::string getOrCompile(const std::string& expression, 
                            const std::function<std::string(const std::string&)>& compileFunc);
    
    /**
     * @brief Clear cache
     */
    void clear();
    
    /**
     * @brief Get cache size
     */
    size_t size() const { return cache_.size(); }
};

} // namespace fastrules