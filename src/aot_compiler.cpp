#include "fastrules/aot_compiler.hpp"
#include "fastrules/logger.hpp"

#include <lua.hpp>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cstring>

namespace fastrules {

AotCompiler::AotCompiler(const std::filesystem::path& cacheDir) 
    : cacheDirectory_(cacheDir) {
    // Create cache directory if it doesn't exist
    if (useDiskCache_ && !cacheDirectory_.empty()) {
        try {
            std::filesystem::create_directories(cacheDirectory_);
        } catch (...) {
            useDiskCache_ = false;
            auto log = logger();
            if (log) {
                log->warn("Failed to create AOT cache directory: {}", cacheDirectory_.string());
            }
        }
    }
}

std::string AotCompiler::compileToBytecode(const std::string& expression, lua_State* lua) {
    if (!lua) {
        throw std::invalid_argument("Lua state is null");
    }

    // Compile the expression to bytecode
    if (luaL_loadstring(lua, expression.c_str()) != LUA_OK) {
        std::string error = lua_tostring(lua, -1);
        lua_pop(lua, 1);
        throw std::runtime_error("Failed to compile expression: " + error);
    }

    // Dump the compiled function to bytecode
    std::stringstream bytecodeStream;
    if (lua_dump(lua, [](lua_State*, const void* data, size_t size, void* userdata) -> int {
        auto* stream = static_cast<std::stringstream*>(userdata);
        stream->write(static_cast<const char*>(data), size);
        return 0;
    }, &bytecodeStream, 0) != 0) {
        lua_pop(lua, 1);
        throw std::runtime_error("Failed to dump compiled function to bytecode");
    }

    // Clean up the stack
    lua_pop(lua, 1);

    return bytecodeStream.str();
}

const AotCompiledChunk& AotCompiler::getOrCompile(const std::string& expression, lua_State* lua) {
    // Check memory cache first
    auto it = cache_.find(expression);
    if (it != cache_.end()) {
        it->second.hitCount++;
        return it->second;
    }

    // Check disk cache if enabled
    if (useDiskCache_) {
        std::filesystem::path cacheFile = cacheDirectory_ / (std::to_string(std::hash<std::string>{}(expression)) + ".frc");
        if (std::filesystem::exists(cacheFile)) {
            try {
                std::ifstream file(cacheFile, std::ios::binary);
                if (file.is_open()) {
                    std::stringstream buffer;
                    buffer << file.rdbuf();
                    std::string bytecode = buffer.str();

                    AotCompiledChunk chunk;
                    chunk.bytecode = bytecode;
                    chunk.compiledAt = std::chrono::system_clock::now();
                    chunk.hitCount = 1;

                    auto result = cache_.emplace(expression, std::move(chunk));
                    return result.first->second;
                }
            } catch (...) {
                // Ignore disk cache errors
            }
        }
    }

    // Compile and cache
    std::string bytecode = compileToBytecode(expression, lua);

    AotCompiledChunk chunk;
    chunk.bytecode = std::move(bytecode);
    chunk.compiledAt = std::chrono::system_clock::now();
    chunk.hitCount = 1;

    auto result = cache_.emplace(expression, std::move(chunk));
    
    // Save to disk cache if enabled
    if (useDiskCache_) {
        try {
            std::filesystem::path cacheFile = cacheDirectory_ / (std::to_string(std::hash<std::string>{}(expression)) + ".frc");
            std::ofstream file(cacheFile, std::ios::binary);
            if (file.is_open()) {
                file << result.first->second.bytecode;
            }
        } catch (...) {
            // Ignore disk cache errors
        }
    }

    return result.first->second;
}

void AotCompiler::precompile(const std::vector<std::string>& expressions, lua_State* lua) {
    for (const auto& expr : expressions) {
        try {
            getOrCompile(expr, lua);
        } catch (...) {
            // Log error but continue with other expressions
            auto log = logger();
            if (log) {
                log->warn("Failed to precompile expression: {}", expr);
            }
        }
    }
}

void AotCompiler::saveCache() {
    if (!useDiskCache_) return;

    try {
        std::filesystem::path indexFile = cacheDirectory_ / "index.frc";
        std::ofstream file(indexFile);
        if (file.is_open()) {
            for (const auto& [expression, chunk] : cache_) {
                file << std::hash<std::string>{}(expression) << "=" << expression << "\n";
            }
        }
    } catch (...) {
        // Ignore errors
    }
}

void AotCompiler::loadCache() {
    if (!useDiskCache_) return;

    try {
        std::filesystem::path indexFile = cacheDirectory_ / "index.frc";
        if (std::filesystem::exists(indexFile)) {
            std::ifstream file(indexFile);
            if (file.is_open()) {
                std::string line;
                while (std::getline(file, line)) {
                    // In a real implementation, we would load the actual cache entries
                    // This is a simplified version
                }
            }
        }
    } catch (...) {
        // Ignore errors
    }
}

void AotCompiler::clearCache() {
    cache_.clear();
    
    if (useDiskCache_) {
        try {
            for (const auto& entry : std::filesystem::directory_iterator(cacheDirectory_)) {
                if (entry.is_regular_file() && entry.path().extension() == ".frc") {
                    std::filesystem::remove(entry.path());
                }
            }
        } catch (...) {
            // Ignore errors
        }
    }
}

std::pair<double, size_t> AotCompiler::getCacheStats() const {
    if (cache_.empty()) return {0.0, 0};

    size_t totalHits = 0;
    for (const auto& [_, chunk] : cache_) {
        totalHits += chunk.hitCount;
    }

    double hitRatio = static_cast<double>(totalHits) / cache_.size();
    return {hitRatio, cache_.size()};
}

std::string LuaExpressionCache::getOrCompile(const std::string& expression,
                                            const std::function<std::string(const std::string&)>& compileFunc) {
    auto it = cache_.find(expression);
    if (it != cache_.end()) {
        return it->second;
    }

    // Check if we need to evict old entries
    if (cache_.size() >= maxCacheSize_) {
        // Simple eviction: remove the first entry
        cache_.erase(cache_.begin());
    }

    std::string compiled = compileFunc(expression);
    cache_[expression] = compiled;
    return compiled;
}

void LuaExpressionCache::clear() {
    cache_.clear();
}

} // namespace fastrules