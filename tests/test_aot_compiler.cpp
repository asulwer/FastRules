#include "fastrules/aot_compiler.hpp"
#include <doctest/doctest.h>
#include <lua.hpp>

TEST_CASE("AotCompiler basic functionality") {
    // Create a Lua state for testing
    lua_State* lua = luaL_newstate();
    REQUIRE(lua != nullptr);
    
    // Load standard libraries
    luaL_openlibs(lua);
    
    fastrules::AotCompiler compiler("test_cache");
    
    // Test compiling a simple expression
    std::string expression = "return 1 + 1";
    std::string bytecode = compiler.compileToBytecode(expression, lua);
    
    CHECK(!bytecode.empty());
    
    // Test getOrCompile
    const auto& chunk = compiler.getOrCompile(expression, lua);
    CHECK(!chunk.bytecode.empty());
    CHECK(chunk.hitCount == 1);
    
    // Test caching - second call should increment hit count
    const auto& chunk2 = compiler.getOrCompile(expression, lua);
    CHECK(chunk2.hitCount == 2);
    
    // Clean up
    lua_close(lua);
}

TEST_CASE("AotCompiler with complex expressions") {
    // Create a Lua state for testing
    lua_State* lua = luaL_newstate();
    REQUIRE(lua != nullptr);
    
    // Load standard libraries
    luaL_openlibs(lua);
    
    fastrules::AotCompiler compiler("test_cache_complex");
    
    // Test compiling a more complex expression
    std::string expression = R"(
        local x = 10
        local y = 20
        return x * y + 5
    )";
    
    std::string bytecode = compiler.compileToBytecode(expression, lua);
    CHECK(!bytecode.empty());
    
    // Test getOrCompile
    const auto& chunk = compiler.getOrCompile(expression, lua);
    CHECK(!chunk.bytecode.empty());
    
    // Clean up
    lua_close(lua);
}

TEST_CASE("AotCompiler error handling") {
    // Create a Lua state for testing
    lua_State* lua = luaL_newstate();
    REQUIRE(lua != nullptr);
    
    // Load standard libraries
    luaL_openlibs(lua);
    
    fastrules::AotCompiler compiler("test_cache_error");
    
    // Test compiling an invalid expression
    std::string invalidExpression = "return 1 +";
    
    CHECK_THROWS_AS(compiler.compileToBytecode(invalidExpression, lua), std::runtime_error);
    
    // Test getOrCompile with invalid expression
    CHECK_THROWS_AS(compiler.getOrCompile(invalidExpression, lua), std::runtime_error);
    
    // Clean up
    lua_close(lua);
}

TEST_CASE("LuaExpressionCache basic functionality") {
    fastrules::LuaExpressionCache cache;
    
    // Test compiling a simple expression
    std::string expression = "return 1 + 1";
    std::string compiled = cache.getOrCompile(expression, [](const std::string& expr) {
        return "compiled_" + expr;
    });
    
    CHECK(compiled == "compiled_return 1 + 1");
    CHECK(cache.size() == 1);
    
    // Test caching - second call should return the same result
    std::string compiled2 = cache.getOrCompile(expression, [](const std::string& expr) {
        return "compiled2_" + expr;
    });
    
    CHECK(compiled2 == "compiled_return 1 + 1"); // Should return cached version
    CHECK(cache.size() == 1);
    
    // Test clearing cache
    cache.clear();
    CHECK(cache.size() == 0);
}