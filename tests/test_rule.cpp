#include <doctest/doctest.h>
#include <fastrules.hpp>
#include <thread>

using namespace fastrules;

TEST_CASE("Rule basic creation") {
    Rule rule;
    rule.id = 1;
    rule.description = "Test rule";
    rule.expression = "true";

    REQUIRE(rule.id == 1);
    REQUIRE(rule.description == "Test rule");
    REQUIRE(rule.isActive == true);
    REQUIRE(rule.priority == 0);
}

TEST_CASE("Rule predicate factories") {
    SUBCASE("isNotNull") {
        auto rule = Rule::isNotNull("customer");
        REQUIRE(rule.expression.find("isNotNull") != std::string::npos);
    }

    SUBCASE("greaterThan") {
        auto rule = Rule::greaterThan("age", 18);
        REQUIRE(rule.expression.find("> 18") != std::string::npos);
    }

    SUBCASE("lessThan") {
        auto rule = Rule::lessThan("age", 65);
        REQUIRE(rule.expression.find("< 65") != std::string::npos);
    }

    SUBCASE("equals") {
        auto rule = Rule::equals("status", "active");
        REQUIRE(rule.expression.find("== \"active\"") != std::string::npos);
    }

    SUBCASE("matchesRegex") {
        auto rule = Rule::matchesRegex("email", "@");
        REQUIRE(rule.expression.find("matchesRegex") != std::string::npos);
    }

    SUBCASE("contains") {
        auto rule = Rule::contains("name", "John");
        REQUIRE(rule.expression.find("string.find") != std::string::npos);
    }
}

TEST_CASE("Rule compile and execute") {
    LuaEngine engine;

    auto rule = std::make_shared<Rule>();
    rule->id = 1;
    rule->expression = "value > 10";
    rule->action = "result = true";

    rule->compile(engine);

    REQUIRE(rule->expression == "value > 10");
}

TEST_CASE("Rule inactive") {
    LuaEngine engine;
    RuleContext ctx;

    auto rule = std::make_shared<Rule>();
    rule->id = 1;
    rule->isActive = false;
    rule->expression = "false";

    rule->compile(engine);

    std::vector<RuleParameter> params;
    auto result = rule->execute(engine, ctx, params);

    REQUIRE(result.isSuccess() == false);
    REQUIRE(result.skipped == true);
}

TEST_CASE("Rule timeout enforcement") {
    LuaEngine engine;
    RuleContext ctx;

    auto rule = std::make_shared<Rule>();
    rule->id = 1;
    rule->expression = "true";
    rule->timeout = std::chrono::milliseconds(5000);

    rule->compile(engine);

    std::vector<RuleParameter> params;
    auto result = rule->execute(engine, ctx, params);

    // Should succeed - not timing out
    REQUIRE(result.isSuccess() == true);
    REQUIRE(result.exception.has_value() == false);
}

TEST_CASE("Rule timeout not exceeded") {
    LuaEngine engine;
    RuleContext ctx;

    auto rule = std::make_shared<Rule>();
    rule->id = 1;
    rule->expression = "true";
    rule->timeout = std::chrono::milliseconds(5000);

    rule->compile(engine);

    std::vector<RuleParameter> params;
    auto result = rule->execute(engine, ctx, params);

    REQUIRE(result.isSuccess() == true);
    REQUIRE(result.exception.has_value() == false);
}

TEST_CASE("Rule cache memoization") {
    LuaEngine engine;
    RuleContext ctx;

    auto rule = std::make_shared<Rule>();
    rule->id = 1;
    rule->expression = "true";
    rule->cacheDuration = std::chrono::milliseconds(1000);

    rule->compile(engine);

    std::vector<RuleParameter> params;
    params.emplace_back("value", 42);

    auto result1 = rule->execute(engine, ctx, params);
    REQUIRE(result1.isSuccess() == true);
    auto time1 = result1.executedAt;

    auto result2 = rule->execute(engine, ctx, params);
    REQUIRE(result2.isSuccess() == true);
    
    REQUIRE(result2.executedAt == time1);
}

TEST_CASE("Rule cache expires") {
    LuaEngine engine;
    RuleContext ctx;

    auto rule = std::make_shared<Rule>();
    rule->id = 1;
    rule->expression = "true";
    rule->cacheDuration = std::chrono::milliseconds(50);

    rule->compile(engine);

    std::vector<RuleParameter> params;
    params.emplace_back("value", 42);

    auto result1 = rule->execute(engine, ctx, params);
    REQUIRE(result1.isSuccess() == true);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto result2 = rule->execute(engine, ctx, params);
    REQUIRE(result2.isSuccess() == true);
    
    REQUIRE(result2.executedAt > result1.executedAt);
}

TEST_CASE("Rule child rules execution") {
    LuaEngine engine;
    RuleContext ctx;

    auto parent = std::make_shared<Rule>();
    parent->id = 1;
    parent->expression = "true";

    auto child1 = std::make_shared<Rule>();
    child1->id = 2;
    child1->expression = "true";

    auto child2 = std::make_shared<Rule>();
    child2->id = 3;
    child2->expression = "true";

    parent->childRules.push_back(child1);
    parent->childRules.push_back(child2);

    parent->compile(engine);

    std::vector<RuleParameter> params;
    auto result = parent->execute(engine, ctx, params);

    REQUIRE(!result.childResults.empty());
    REQUIRE(result.childResults.size() == 2);
    REQUIRE(result.isFullySuccessful() == true);
}

TEST_CASE("Rule cache invalidation on property change") {
    LuaEngine engine;
    RuleContext ctx;

    auto rule = Rule::Builder(1)
        .withExpression("true")
        .withCacheDuration(std::chrono::milliseconds(1000))
        .build();

    rule->compile(engine);
    std::vector<RuleParameter> params;

    auto result1 = rule->execute(engine, ctx, params);
    REQUIRE(result1.isSuccess() == true);

    auto result2 = rule->execute(engine, ctx, params);
    REQUIRE(result2.isSuccess() == true);
    REQUIRE(result2.executedAt == result1.executedAt);

    rule->expression = "false";
    rule->invalidateCache();

    rule->compile(engine);

    auto result3 = rule->execute(engine, ctx, params);
    REQUIRE(result3.isSuccess() == false);
}

TEST_CASE("Rule cache expires after TTL") {
    LuaEngine engine;
    RuleContext ctx;

    auto rule = Rule::Builder(1)
        .withExpression("true")
        .withCacheDuration(std::chrono::milliseconds(50))
        .build();

    rule->compile(engine);
    std::vector<RuleParameter> params;

    auto result1 = rule->execute(engine, ctx, params);
    REQUIRE(result1.isSuccess() == true);

    auto result2 = rule->execute(engine, ctx, params);
    REQUIRE(result2.isSuccess() == true);
    REQUIRE(result2.executedAt == result1.executedAt);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto result3 = rule->execute(engine, ctx, params);
    REQUIRE(result3.isSuccess() == true);
    REQUIRE(result3.executedAt > result1.executedAt);
}
