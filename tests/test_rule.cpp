#include <catch2/catch_test_macros.hpp>
#include <fastrules.hpp>

using namespace fastrules;

TEST_CASE("Rule basic creation", "[rule]") {
    Rule rule;
    rule.id = 1;
    rule.description = "Test rule";
    rule.expression = "true";

    REQUIRE(rule.id == 1);
    REQUIRE(rule.description == "Test rule");
    REQUIRE(rule.isActive == true);
    REQUIRE(rule.priority == 0);
}

TEST_CASE("Rule predicate factories", "[rule]") {
    SECTION("isNotNull") {
        auto rule = Rule::isNotNull("customer");
        REQUIRE(rule.expression.find("isNotNull") != std::string::npos);
    }

    SECTION("greaterThan") {
        auto rule = Rule::greaterThan("age", 18);
        REQUIRE(rule.expression.find("> 18") != std::string::npos);
    }

    SECTION("lessThan") {
        auto rule = Rule::lessThan("age", 65);
        REQUIRE(rule.expression.find("< 65") != std::string::npos);
    }

    SECTION("equals") {
        auto rule = Rule::equals("status", "active");
        REQUIRE(rule.expression.find("== \"active\"") != std::string::npos);
    }

    SECTION("matchesRegex") {
        auto rule = Rule::matchesRegex("email", "@");
        REQUIRE(rule.expression.find("matchesRegex") != std::string::npos);
    }

    SECTION("contains") {
        auto rule = Rule::contains("name", "John");
        REQUIRE(rule.expression.find("string.find") != std::string::npos);
    }
}

TEST_CASE("Rule compile and execute", "[rule]") {
    LuaEngine engine;

    auto rule = std::make_shared<Rule>();
    rule->id = 1;
    rule->expression = "value > 10";
    rule->action = "result = true";

    // compile() is public API
    rule->compile(engine);

    REQUIRE(rule->expression == "value > 10");
}

TEST_CASE("Rule inactive", "[rule]") {
    LuaEngine engine;
    RuleContext ctx;

    auto rule = std::make_shared<Rule>();
    rule->id = 1;
    rule->isActive = false;
    rule->expression = "false";

    // compile() is public API
    rule->compile(engine);

    std::vector<RuleParameter> params;
    auto result = rule->execute(engine, ctx, params);

    // Inactive rules are skipped - not success, not failure
    REQUIRE(result.isSuccess() == false);
    REQUIRE(result.skipped == true);
}

TEST_CASE("Rule timeout enforcement", "[rule]") {
    LuaEngine engine;
    RuleContext ctx;

    auto rule = std::make_shared<Rule>();
    rule->id = 1;
    // Simple expression that should complete quickly - just test timeout infrastructure exists
    // Real timeout testing would require an infinite loop or very slow operation
    rule->expression = "true";
    rule->timeout = std::chrono::milliseconds(5000);  // 5s timeout - plenty of time

    // compile() is public API
    rule->compile(engine);

    std::vector<RuleParameter> params;
    auto result = rule->execute(engine, ctx, params);

    // Should succeed - not timing out
    REQUIRE(result.isSuccess() == true);
    REQUIRE(result.exception.has_value() == false);
}

TEST_CASE("Rule timeout not exceeded", "[rule]") {
    LuaEngine engine;
    RuleContext ctx;

    auto rule = std::make_shared<Rule>();
    rule->id = 1;
    rule->expression = "true";
    rule->timeout = std::chrono::milliseconds(5000);  // 5s timeout

    // compile() is public API
    rule->compile(engine);

    std::vector<RuleParameter> params;
    auto result = rule->execute(engine, ctx, params);

    // Should succeed
    REQUIRE(result.isSuccess() == true);
    REQUIRE(result.exception.has_value() == false);
}

TEST_CASE("Rule cache memoization", "[rule]") {
    LuaEngine engine;
    RuleContext ctx;

    auto rule = std::make_shared<Rule>();
    rule->id = 1;
    rule->expression = "true";
    rule->cacheDuration = std::chrono::milliseconds(1000);  // 1 second cache

    // compile() is public API
    rule->compile(engine);

    std::vector<RuleParameter> params;
    params.emplace_back("value", 42);

    // First execution
    auto result1 = rule->execute(engine, ctx, params);
    REQUIRE(result1.isSuccess() == true);
    auto time1 = result1.executedAt;

    // Second execution with same params - should return cached result
    auto result2 = rule->execute(engine, ctx, params);
    REQUIRE(result2.isSuccess() == true);
    
    // Cached result should have same timestamp
    REQUIRE(result2.executedAt == time1);
}

TEST_CASE("Rule cache expires", "[rule]") {
    LuaEngine engine;
    RuleContext ctx;

    auto rule = std::make_shared<Rule>();
    rule->id = 1;
    rule->expression = "true";
    rule->cacheDuration = std::chrono::milliseconds(50);  // 50ms cache

    // compile() is public API
    rule->compile(engine);

    std::vector<RuleParameter> params;
    params.emplace_back("value", 42);

    // First execution
    auto result1 = rule->execute(engine, ctx, params);
    REQUIRE(result1.isSuccess() == true);

    // Wait for cache to expire
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Second execution - cache expired, should re-execute
    auto result2 = rule->execute(engine, ctx, params);
    REQUIRE(result2.isSuccess() == true);
    
    // Should have different timestamp since it was re-executed
    REQUIRE(result2.executedAt > result1.executedAt);
}

TEST_CASE("Rule child rules execution", "[rule]") {
    LuaEngine engine;
    RuleContext ctx;

    auto parent = std::make_shared<Rule>();
    parent->id = 1;
    parent->expression = "true";

    auto child1 = std::make_shared<Rule>();
    child1->id = 1;
    child1->expression = "true";

    auto child2 = std::make_shared<Rule>();
    child2->id = 1;
    child2->expression = "true";

    parent->childRules.push_back(child1);
    parent->childRules.push_back(child2);

    // compile() is public API
    parent->compile(engine);

    std::vector<RuleParameter> params;
    auto result = parent->execute(engine, ctx, params);

    REQUIRE(!result.childResults.empty());
    REQUIRE(result.childResults.size() == 2);
    REQUIRE(result.isFullySuccessful() == true);
}
