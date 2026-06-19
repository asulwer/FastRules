/**
 * @file aot_compiler_example.cpp
 * @brief Example demonstrating Ahead-of-Time compilation with FastRules
 * 
 * This example shows how to use the AOT compiler to pre-compile Lua expressions
 * for better performance.
 */

#include "fastrules/aot_compiler.hpp"
#include "fastrules/lua_engine.hpp"
#include <iostream>
#include <vector>
#include <chrono>

using namespace fastrules;

int main() {
    std::cout << "FastRules AOT Compiler Example\n";
    std::cout << "==============================\n\n";
    
    try {
        // Create a Lua engine
        LuaEngine engine;
        lua_State* lua = engine.luaState();
        
        // Create AOT compiler with cache directory
        AotCompiler compiler(".fastrules_cache");
        
        // Example 1: Basic AOT compilation
        std::cout << "1. Basic AOT compilation:\n";
        std::string simpleExpr = "return x + y";
        const auto& chunk = compiler.getOrCompile(simpleExpr, lua);
        std::cout << "   Expression: " << simpleExpr << "\n";
        std::cout << "   Bytecode size: " << chunk.bytecode.size() << " bytes\n";
        std::cout << "   Hit count: " << chunk.hitCount << "\n\n";
        
        // Example 2: Pre-compiling common expressions
        std::cout << "2. Pre-compiling common expressions:\n";
        std::vector<std::string> commonExpressions = {
            "return value > 10",
            "return value * 2",
            "return value + 100",
            "return math.sqrt(value)",
            "return value % 2 == 0"
        };
        
        // Pre-compile all expressions
        auto start = std::chrono::high_resolution_clock::now();
        compiler.precompile(commonExpressions, lua);
        auto end = std::chrono::high_resolution_clock::now();
        
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        std::cout << "   Pre-compiled " << commonExpressions.size() << " expressions in " 
                  << duration.count() << " microseconds\n\n";
        
        // Example 3: Using cached expressions
        std::cout << "3. Using cached expressions:\n";
        for (const auto& expr : commonExpressions) {
            const auto& cachedChunk = compiler.getOrCompile(expr, lua);
            std::cout << "   " << expr << " (hit count: " << cachedChunk.hitCount << ")\n";
        }
        std::cout << "\n";
        
        // Example 4: Cache statistics
        std::cout << "4. Cache statistics:\n";
        auto [hitRatio, cacheSize] = compiler.getCacheStats();
        std::cout << "   Cache size: " << cacheSize << " entries\n";
        std::cout << "   Hit ratio: " << hitRatio << "\n\n";
        
        // Example 5: Expression caching with LuaExpressionCache
        std::cout << "5. Expression caching with LuaExpressionCache:\n";
        LuaExpressionCache exprCache;
        
        std::string testExpr = "return value * 3 + 7";
        std::string compiledExpr = exprCache.getOrCompile(testExpr, [](const std::string& expr) {
            // Simulate compilation
            return "COMPILED:" + expr;
        });
        
        std::cout << "   Original expression: " << testExpr << "\n";
        std::cout << "   Compiled expression: " << compiledExpr << "\n";
        std::cout << "   Cache size: " << exprCache.size() << " entries\n\n";
        
        std::cout << "AOT compiler example completed successfully!\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}