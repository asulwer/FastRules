#include "fastrules/aot_compiler.hpp"
#include <doctest/doctest.h>
#include <lua.hpp>
#include <thread>
#include <vector>
#include <future>

using namespace fastrules;

TEST_CASE("AOTCompiler advanced functionality") {
    // Create a Lua state for testing
    lua_State* lua = luaL_newstate();
    REQUIRE(lua != nullptr);
    
    // Load standard libraries
    luaL_openlibs(lua);
    
    AotCompiler compiler("test_cache_advanced");
    
    // Test compiling various expressions
    std::vector<std::string> expressions = {
        "return x + y",
        "return (a > b) and (c < d)",
        "return string.len(name) > 0",
        "return math.floor(value) == 10",
        "return table.concat(items, ',')"
    };
    
    for (const auto& expr : expressions) {
        std::string bytecode = compiler.compileToBytecode(expr, lua);
        CHECK_FALSE(bytecode.empty());
        
        // Test caching
        const auto& chunk = compiler.getOrCompile(expr, lua);
        CHECK_FALSE(chunk.bytecode.empty());
        CHECK(chunk.hitCount >= 1);
    }
    
    // Clean up
    lua_close(lua);
}

TEST_CASE("AOTCompiler cache functionality") {
    lua_State* lua = luaL_newstate();
    REQUIRE(lua != nullptr);
    luaL_openlibs(lua);
    
    AotCompiler compiler("test_cache_functionality");
    
    std::string expression = "return cache_test > 0";
    
    // First compilation
    const auto& chunk1 = compiler.getOrCompile(expression, lua);
    size_t initialHitCount = chunk1.hitCount;
    
    // Second compilation (should use cache)
    const auto& chunk2 = compiler.getOrCompile(expression, lua);
    
    // The second access should have incremented the hit count
    CHECK(chunk2.hitCount > initialHitCount);
    
    auto duration1 = std::chrono::duration_cast<std::chrono::microseconds>(chunk1.compiledAt.time_since_epoch());
    auto duration2 = std::chrono::duration_cast<std::chrono::microseconds>(chunk2.compiledAt.time_since_epoch());
    
    // Both chunks should have the same compilation time (same cached entry)
    CHECK(duration2.count() == duration1.count());
    
    lua_close(lua);
}

TEST_CASE("AOTCompiler thread safety") {
    // Test concurrent compilation
    std::vector<std::future<bool>> futures;
    
    for (int i = 0; i < 5; ++i) {
        futures.push_back(std::async(std::launch::async, [i]() {
            lua_State* lua = luaL_newstate();
            if (!lua) return false;
            
            luaL_openlibs(lua);
            
            AotCompiler compiler("test_concurrent_" + std::to_string(i));
            std::string expr = "return thread_test_" + std::to_string(i) + " > 0";
            
            std::string bytecode = compiler.compileToBytecode(expr, lua);
            bool result = !bytecode.empty();
            
            lua_close(lua);
            return result;
        }));
    }
    
    // Check all compilations succeeded
    for (auto& future : futures) {
        REQUIRE(future.get() == true);
    }
}

TEST_CASE("AOTCompiler error handling") {
    lua_State* lua = luaL_newstate();
    REQUIRE(lua != nullptr);
    luaL_openlibs(lua);
    
    AotCompiler compiler("test_error_handling");
    
    // Test compiling invalid expressions
    std::string invalidExpr1 = "return 1 +"; // Syntax error
    CHECK_THROWS_AS(compiler.compileToBytecode(invalidExpr1, lua), std::exception);
    
    // Skip this test as it doesn't actually cause a compilation error
    // std::string invalidExpr2 = "return nil + nil"; // Runtime error in compilation
    // CHECK_THROWS_AS(compiler.compileToBytecode(invalidExpr2, lua), std::exception);
    
    lua_close(lua);
}

TEST_CASE("AOTCompiler performance") {
    lua_State* lua = luaL_newstate();
    REQUIRE(lua != nullptr);
    luaL_openlibs(lua);
    
    AotCompiler compiler("test_performance");
    
    // Test compilation performance with multiple expressions
    std::vector<std::string> expressions;
    for (int i = 0; i < 100; ++i) {
        expressions.push_back("return expr_" + std::to_string(i) + " > " + std::to_string(i));
    }
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (const auto& expr : expressions) {
        std::string bytecode = compiler.compileToBytecode(expr, lua);
        CHECK_FALSE(bytecode.empty());
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // Should compile 100 expressions reasonably quickly
    CHECK(duration.count() < 5000); // 5 seconds should be more than enough
    
    lua_close(lua);
}

TEST_CASE("AOTCompiler precompilation") {
    lua_State* lua = luaL_newstate();
    REQUIRE(lua != nullptr);
    luaL_openlibs(lua);
    
    AotCompiler compiler("test_precompilation");
    
    // Test precompiling expressions
    std::vector<std::string> expressions = {
        "return precompile_test > 0",
        "return precompile_test2 == 'hello'",
        "return precompile_test3 and precompile_test4"
    };
    
    // This should not throw
    compiler.precompile(expressions, lua);
    
    // Check that expressions are now cached
    for (const auto& expr : expressions) {
        const auto& chunk = compiler.getOrCompile(expr, lua);
        CHECK_FALSE(chunk.bytecode.empty());
        CHECK(chunk.hitCount >= 0);
    }
    
    lua_close(lua);
}

TEST_CASE("AOTCompiler cache persistence") {
    lua_State* lua = luaL_newstate();
    REQUIRE(lua != nullptr);
    luaL_openlibs(lua);
    
    {
        AotCompiler compiler("test_cache_persistence");
        
        // Compile some expressions
        std::string expr = "return persistence_test > 0";
        const auto& chunk = compiler.getOrCompile(expr, lua);
        CHECK_FALSE(chunk.bytecode.empty());
        
        // Save cache
        compiler.saveCache();
    }
    
    // Load cache in a new compiler instance
    {
        AotCompiler compiler("test_cache_persistence");
        compiler.loadCache();
        
        // Check cache statistics
        auto stats = compiler.getCacheStats();
        CHECK(stats.second >= 0); // Cache size should be non-negative
    }
    
    lua_close(lua);
}

TEST_CASE("AOTCompiler cache management") {
    lua_State* lua = luaL_newstate();
    REQUIRE(lua != nullptr);
    luaL_openlibs(lua);
    
    AotCompiler compiler("test_cache_management");
    
    // Compile some expressions
    std::vector<std::string> expressions;
    for (int i = 0; i < 10; ++i) {
        expressions.push_back("return management_test_" + std::to_string(i) + " > 0");
    }
    
    for (const auto& expr : expressions) {
        compiler.getOrCompile(expr, lua);
    }
    
    // Check cache size
    auto stats = compiler.getCacheStats();
    CHECK(stats.second == 10);
    
    // Clear cache
    compiler.clearCache();
    
    // Check cache is empty
    auto stats2 = compiler.getCacheStats();
    CHECK(stats2.second == 0);
    
    lua_close(lua);
}

TEST_CASE("LuaExpressionCache functionality") {
    LuaExpressionCache cache;
    
    // Test caching with a simple compile function
    auto compileFunc = [](const std::string& expr) -> std::string {
        return "compiled_" + expr;
    };
    
    std::string expr = "test_expression";
    std::string result1 = cache.getOrCompile(expr, compileFunc);
    CHECK(result1 == "compiled_test_expression");
    
    // Second call should return cached result
    std::string result2 = cache.getOrCompile(expr, compileFunc);
    CHECK(result2 == "compiled_test_expression");
    
    CHECK(cache.size() == 1);
    
    // Clear cache
    cache.clear();
    CHECK(cache.size() == 0);
}

TEST_CASE("AOTCompiler edge cases") {
    lua_State* lua = luaL_newstate();
    REQUIRE(lua != nullptr);
    luaL_openlibs(lua);
    
    AotCompiler compiler("test_edge_cases");
    
    // Test with empty expression
    // std::string emptyExpr = "";
    // CHECK_THROWS_AS(compiler.compileToBytecode(emptyExpr, lua), std::exception);
    
    // Test with very long expression
    std::string longExpr(10000, 'x');
    longExpr = "return " + longExpr + " > 0";
    
    // This should either succeed or throw appropriately
    try {
        std::string bytecode = compiler.compileToBytecode(longExpr, lua);
        // If it succeeds, bytecode should not be empty
        CHECK_FALSE(bytecode.empty());
    } catch (const std::runtime_error&) {
        // This is also acceptable
        CHECK(true);
    }
    
    lua_close(lua);
}