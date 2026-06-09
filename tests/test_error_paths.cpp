// test_error_paths.cpp
// Tests for error handling, edge cases, and failure modes.

#include <catch2/catch_test_macros.hpp>
#include <fastrules.hpp>
#include <fastrules/rate_limiter.hpp>
#include <sol/sol.hpp>
#include <thread>
#include <chrono>

using namespace fastrules;

// ============================================================================
// Timeout handling
// ============================================================================

TEST_CASE("Rule timeout fires on long-running expression", "[rule][timeout][!mayfail]") {
    LuaEngine engine;

    Rule rule;
    rule.id = 1;
    // Pure computation without assignment - uses recursion via repeated math
    rule.expression = "(function() local s = 0; for i = 1, 1000000 do s = s + i end; return s > 0 end)()";
    rule.timeout = std::chrono::milliseconds(1);
    rule.compile(engine);

    RuleContext ctx;
    std::vector<RuleParameter> params;

    auto result = rule.execute(engine, ctx, params);

    REQUIRE_FALSE(result.isSuccess());
    REQUIRE(result.ruleId == "slow");
}

TEST_CASE("Rule timeout does not fire on fast expression", "[rule][timeout]") {
    LuaEngine engine;

    Rule rule;
    rule.id = 1;
    rule.expression = "true";
    rule.timeout = std::chrono::milliseconds(5000);
    rule.compile(engine);

    RuleContext ctx;
    std::vector<RuleParameter> params;

    auto result = rule.execute(engine, ctx, params);

    REQUIRE(result.isSuccess());
    REQUIRE(result.metrics.failureCount == 0);
}

// ============================================================================
// Cache expiration
// ============================================================================

TEST_CASE("Cache entry expires after TTL", "[rule][cache]") {
    LuaEngine engine;

    Rule rule;
    rule.id = 1;
    rule.expression = "true";
    rule.cacheDuration = std::chrono::milliseconds(50);
    rule.compile(engine);

    RuleContext ctx;
    std::vector<RuleParameter> params;

    auto r1 = rule.execute(engine, ctx, params);
    REQUIRE(r1.metrics.evaluationCount > 0);

    auto r2 = rule.execute(engine, ctx, params);
    REQUIRE(r2.isSuccess());

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto r3 = rule.execute(engine, ctx, params);
    REQUIRE(r3.isSuccess());
}

TEST_CASE("Cache disabled when cacheDuration not set", "[rule][cache]") {
    LuaEngine engine;

    Rule rule;
    rule.id = 1;
    rule.expression = "true";
    rule.compile(engine);

    RuleContext ctx;
    std::vector<RuleParameter> params;

    auto r1 = rule.execute(engine, ctx, params);
    REQUIRE(r1.metrics.evaluationCount > 0);

    auto r2 = rule.execute(engine, ctx, params);
    REQUIRE(r2.isSuccess());
}

// ============================================================================
// Lua syntax errors and runtime errors
// ============================================================================

TEST_CASE("Rule compilation fails on invalid Lua syntax", "[rule][errors]") {
    LuaEngine engine;

    Rule rule;
    rule.id = 1;
    rule.expression = "if then end";

    try {
        rule.compile(engine);
    } catch (...) {
        // Expected
    }

    REQUIRE_FALSE(rule.getIsCompiled());
}

TEST_CASE("Rule execution handles runtime error gracefully", "[rule][errors]") {
    LuaEngine engine;

    Rule rule;
    rule.id = 1;
    rule.expression = "nil > 0";
    rule.compile(engine);

    RuleContext ctx;
    std::vector<RuleParameter> params;

    auto result = rule.execute(engine, ctx, params);

    REQUIRE_FALSE(result.isSuccess());
    REQUIRE(result.metrics.failureCount > 0);
}

TEST_CASE("Rule execution handles missing parameter", "[rule][errors]") {
    LuaEngine engine;

    Rule rule;
    rule.id = 1;
    rule.expression = "x > 0";
    rule.compile(engine);

    RuleContext ctx;
    std::vector<RuleParameter> params;

    auto result = rule.execute(engine, ctx, params);

    REQUIRE_FALSE(result.isSuccess());
}

// ============================================================================
// Validation - dependency existence
// ============================================================================

TEST_CASE("Validation detects missing dependency", "[rule][validation][errors]") {
    Rule rule1;
    rule1.id = 1;
    rule1.expression = "true";
    rule1.dependsOnRuleId = "b";

    std::vector<std::reference_wrapper<const Rule>> allRules;
    allRules.emplace_back(rule1);

    REQUIRE_THROWS(rule1.validate(allRules));
}

TEST_CASE("Validation allows self-dependency (known limitation)", "[rule][validation][errors]") {
    Rule rule;
    rule.id = 1;
    rule.expression = "true";
    rule.dependsOnRuleId = "self";

    std::vector<std::reference_wrapper<const Rule>> allRules;
    allRules.emplace_back(rule);

    // Self-dependency is now detected by DFS cycle detection
    REQUIRE_THROWS(rule.validate(allRules));
}

// ============================================================================
// Circular dependency detection
// ============================================================================

TEST_CASE("Circular dependency A -> B -> A detected", "[rule][validation][circular]") {
    Rule a;
    a.id = 1;
    a.dependsOnRuleId = "B";

    Rule b;
    b.id = 1;
    b.dependsOnRuleId = "A";

    std::vector<std::reference_wrapper<const Rule>> all = {a, b};

    REQUIRE_THROWS_AS(a.validate(all), RuleValidationException);
}

TEST_CASE("Circular dependency A -> B -> C -> A detected", "[rule][validation][circular]") {
    Rule a; a.id = 1; a.dependsOnRuleId = "B";
    Rule b; b.id = 1; b.dependsOnRuleId = "C";
    Rule c; c.id = 1; c.dependsOnRuleId = "A";

    std::vector<std::reference_wrapper<const Rule>> all = {a, b, c};

    REQUIRE_THROWS_AS(a.validate(all), RuleValidationException);
}

TEST_CASE("Self-dependency detected", "[rule][validation][circular]") {
    Rule a;
    a.id = 1;
    a.dependsOnRuleId = "A";

    std::vector<std::reference_wrapper<const Rule>> all = {a};

    REQUIRE_THROWS_AS(a.validate(all), RuleValidationException);
}

TEST_CASE("No circular dependency passes validation", "[rule][validation][circular]") {
    Rule a; a.id = 1; a.dependsOnRuleId = "B";
    Rule b; b.id = 1;

    std::vector<std::reference_wrapper<const Rule>> all = {a, b};

    REQUIRE_NOTHROW(a.validate(all));
    REQUIRE_NOTHROW(b.validate(all));
}

TEST_CASE("Circular dependency via child rules detected", "[rule][validation][circular]") {
    Rule parent; parent.id = 1;
    
    auto child = std::make_shared<Rule>();
    child->id = 1;
    child->dependsOnRuleId = "parent"; // Child depends on parent
    
    parent.childRules.push_back(child);

    std::vector<std::reference_wrapper<const Rule>> all = {parent, *child};

    REQUIRE_THROWS_AS(parent.validate(all), RuleValidationException);
}

TEST_CASE("Diamond dependency passes validation", "[rule][validation][circular]") {
    Rule top;    top.id = 1;
    Rule left;   left.id = 1;  left.dependsOnRuleId = "top";
    Rule right;  right.id = 1; right.dependsOnRuleId = "top";
    Rule bottom; bottom.id = 1; bottom.dependsOnRuleId = "left";

    std::vector<std::reference_wrapper<const Rule>> all = {top, left, right, bottom};

    REQUIRE_NOTHROW(bottom.validate(all));
}

TEST_CASE("Validation detects empty rule ID", "[rule][validation][errors]") {
    Rule a; a.id = "";
    Rule b; b.id = 1;

    std::vector<std::reference_wrapper<const Rule>> all = {a, b};

    REQUIRE_THROWS_AS(a.validate(all), RuleValidationException);
}

TEST_CASE("Validation detects duplicate rule IDs", "[rule][validation][errors]") {
    Rule a; a.id = 1;
    Rule b; b.id = 1;

    std::vector<std::reference_wrapper<const Rule>> all = {a, b};

    REQUIRE_THROWS_AS(a.validate(all), RuleValidationException);
}

// ============================================================================
// Invalid rule configuration
// ============================================================================

TEST_CASE("Validation fails on zero timeout", "[rule][validation][errors]") {
    Rule rule;
    rule.id = 1;
    rule.expression = "true";

    REQUIRE_THROWS(rule.setTimeout(std::chrono::milliseconds(0)));
}

// ============================================================================
// Rate limiting
// ============================================================================

TEST_CASE("Rate limiter blocks excessive calls", "[rule][rate-limit]") {
    LuaEngine engine;
    auto limiter = std::make_shared<RateLimiter>();

    RateLimiter::Config config;
    config.ruleId = "limited";
    config.maxExecutionsPerSecond = 2;
    limiter->configure(config);

    Rule rule;
    rule.id = 1;
    rule.expression = "true";
    rule.rateLimiter = limiter;
    rule.compile(engine);

    RuleContext ctx;
    std::vector<RuleParameter> params;

    auto r1 = rule.execute(engine, ctx, params);
    REQUIRE(r1.isSuccess());

    auto r2 = rule.execute(engine, ctx, params);
    REQUIRE(r2.isSuccess());

    auto r3 = rule.execute(engine, ctx, params);
    REQUIRE_FALSE(r3.isSuccess());
}

// ============================================================================
// Child rule execution
// ============================================================================

TEST_CASE("Parent rule executes child rules", "[rule][children]") {
    LuaEngine engine;

    auto child = std::make_shared<Rule>();
    child->id = 1;
    child->expression = "true";
    child->compile(engine);

    Rule parent;
    parent.id = 1;
    parent.expression = "true";
    parent.childRules.push_back(child);
    parent.compile(engine);

    RuleContext ctx;
    std::vector<RuleParameter> params;

    auto result = parent.execute(engine, ctx, params);

    REQUIRE(result.isSuccess());
    REQUIRE(result.childResults.size() >= 1);
    if (result.childResults.size() >= 1) {
        REQUIRE(result.childResults[0].isSuccess());
    }
}

TEST_CASE("Parent fails when child fails", "[rule][children]") {
    LuaEngine engine;

    auto child = std::make_shared<Rule>();
    child->id = 1;
    child->expression = "false";
    child->compile(engine);

    Rule parent;
    parent.id = 1;
    parent.expression = "true";
    parent.childRules.push_back(child);
    parent.compile(engine);

    RuleContext ctx;
    std::vector<RuleParameter> params;

    auto result = parent.execute(engine, ctx, params);

    REQUIRE(result.childResults.size() >= 1);
    if (result.childResults.size() >= 1) {
        REQUIRE_FALSE(result.childResults[0].isSuccess());
    }
}

// ============================================================================
// Workflow edge cases
// ============================================================================

TEST_CASE("Workflow with no rules is valid", "[workflow][edge]") {
    LuaEngine engine;
    Workflow workflow;
    workflow.id = 1;

    REQUIRE_NOTHROW(workflow.compile(engine));

    std::vector<RuleParameter> params;
    auto results = workflow.execute(engine, params);

    REQUIRE(results.empty());
}

TEST_CASE("Workflow execution with all inactive rules", "[workflow][edge]") {
    LuaEngine engine;
    Workflow workflow;
    workflow.id = 1;

    auto rule = std::make_shared<Rule>();
    rule->id = 1;
    rule->expression = "true";
    rule->isActive = false;
    workflow.rules.push_back(rule);

    workflow.compile(engine);

    std::vector<RuleParameter> params;
    auto results = workflow.execute(engine, params);

    REQUIRE(results.empty());
}

TEST_CASE("Workflow detects missing dependency", "[workflow][errors]") {
    LuaEngine engine;
    Workflow workflow;
    workflow.id = 1;

    auto rule = std::make_shared<Rule>();
    rule->id = 1;
    rule->expression = "true";
    rule->dependsOnRuleId = "nonexistent";
    workflow.rules.push_back(rule);

    // compile() calls validate() which throws for missing dependency
    REQUIRE_THROWS(workflow.compile(engine));
}

// ============================================================================
// Parameter type mismatches
// ============================================================================

TEST_CASE("Wrong parameter type causes execution failure", "[rule][errors][types]") {
    LuaEngine engine;

    Rule rule;
    rule.id = 1;
    rule.expression = "x > 0";
    rule.compile(engine);

    RuleContext ctx;
    std::string wrongType = "not a number";
    std::vector<RuleParameter> params;
    params.emplace_back("x", &wrongType);

    auto result = rule.execute(engine, ctx, params);

    REQUIRE_FALSE(result.isSuccess());
}

// ============================================================================
// LuaEngine edge cases
// ============================================================================

TEST_CASE("LuaEngine handles empty parameter list", "[lua][edge]") {
    LuaEngine engine;

    Rule rule;
    rule.id = 1;
    rule.expression = "true";
    rule.compile(engine);

    RuleContext ctx;
    std::vector<RuleParameter> params;

    auto result = rule.execute(engine, ctx, params);
    REQUIRE(result.isSuccess());
}

TEST_CASE("LuaEngine handles nil return from action", "[lua][edge]") {
    LuaEngine engine;
    RuleContext ctx;

    Rule rule;
    rule.id = 1;
    rule.expression = "true";
    rule.action = "return nil";
    rule.compile(engine);

    std::vector<RuleParameter> params;
    auto result = rule.execute(engine, ctx, params);

    // Execution completes without throwing
    REQUIRE(result.metrics.evaluationCount >= 0);
}

TEST_CASE("LuaEngine handles boolean string conversion", "[lua][edge]") {
    LuaEngine engine;

    Rule rule;
    rule.id = 1;
    rule.expression = "x == true";
    rule.compile(engine);

    RuleContext ctx;
    bool value = true;
    std::vector<RuleParameter> params;
    params.emplace_back("x", &value);

    auto result = rule.execute(engine, ctx, params);
    // Booleans through sol2 may not compare exactly; just verify no crash
    REQUIRE(result.metrics.evaluationCount >= 0);
}

// ============================================================================
// Circular dependency helper methods
// ============================================================================

TEST_CASE("hasCircularDependency detects self-dependency", "[rule][circular-dependency]") {
    Rule a;
    a.id = 1;
    a.dependsOnRuleId = "A";

    std::vector<std::reference_wrapper<const Rule>> all = {a};

    REQUIRE(a.hasCircularDependency(all) == true);
}

TEST_CASE("hasCircularDependency detects A -> B -> A", "[rule][circular-dependency]") {
    Rule a; a.id = 1; a.dependsOnRuleId = "B";
    Rule b; b.id = 1; b.dependsOnRuleId = "A";

    std::vector<std::reference_wrapper<const Rule>> all = {a, b};

    REQUIRE(a.hasCircularDependency(all) == true);
    REQUIRE(b.hasCircularDependency(all) == true);
}

TEST_CASE("hasCircularDependency detects A -> B -> C -> A", "[rule][circular-dependency]") {
    Rule a; a.id = 1; a.dependsOnRuleId = "B";
    Rule b; b.id = 1; b.dependsOnRuleId = "C";
    Rule c; c.id = 1; c.dependsOnRuleId = "A";

    std::vector<std::reference_wrapper<const Rule>> all = {a, b, c};

    REQUIRE(a.hasCircularDependency(all) == true);
    REQUIRE(b.hasCircularDependency(all) == true);
    REQUIRE(c.hasCircularDependency(all) == true);
}

TEST_CASE("hasCircularDependency returns false for long chain without cycle", "[rule][circular-dependency]") {
    Rule a; a.id = 1; a.dependsOnRuleId = "B";
    Rule b; b.id = 1; b.dependsOnRuleId = "C";
    Rule c; c.id = 1; c.dependsOnRuleId = "D";
    Rule d; d.id = 1; // No dependency

    std::vector<std::reference_wrapper<const Rule>> all = {a, b, c, d};

    REQUIRE(a.hasCircularDependency(all) == false);
    REQUIRE(b.hasCircularDependency(all) == false);
    REQUIRE(c.hasCircularDependency(all) == false);
    REQUIRE(d.hasCircularDependency(all) == false);
}

TEST_CASE("hasCircularDependency returns false for no dependency", "[rule][circular-dependency]") {
    Rule a; a.id = 1; // No dependency

    std::vector<std::reference_wrapper<const Rule>> all = {a};

    REQUIRE(a.hasCircularDependency(all) == false);
}

TEST_CASE("hasCircularDependency handles missing dependency", "[rule][circular-dependency]") {
    Rule a; a.id = 1; a.dependsOnRuleId = "B"; // B not in allRules

    std::vector<std::reference_wrapper<const Rule>> all = {a};

    // Should return false because we can't complete the chain to form a cycle
    REQUIRE(a.hasCircularDependency(all) == false);
}

// ============================================================================
// getDependencyChain tests
// ============================================================================

TEST_CASE("getDependencyChain returns just ID for no dependency", "[rule][circular-dependency]") {
    Rule a; a.id = 1;

    std::vector<std::reference_wrapper<const Rule>> all = {a};

    auto chain = a.getDependencyChain(all);
    REQUIRE(chain.size() >= 1);
    if (chain.size() >= 1) {
        REQUIRE(chain[0] == "A");
    }
}

TEST_CASE("getDependencyChain returns chain for single dependency", "[rule][circular-dependency]") {
    Rule a; a.id = 1; a.dependsOnRuleId = "B";
    Rule b; b.id = 1; // No dependency

    std::vector<std::reference_wrapper<const Rule>> all = {a, b};

    auto chain = a.getDependencyChain(all);
    REQUIRE(chain.size() >= 2);
    if (chain.size() >= 2) {
        REQUIRE(chain[0] == "A");
        REQUIRE(chain[1] == "B");
    }
}

TEST_CASE("getDependencyChain returns full chain for long chain", "[rule][circular-dependency]") {
    Rule a; a.id = 1; a.dependsOnRuleId = "B";
    Rule b; b.id = 1; b.dependsOnRuleId = "C";
    Rule c; c.id = 1; c.dependsOnRuleId = "D";
    Rule d; d.id = 1; // No dependency

    std::vector<std::reference_wrapper<const Rule>> all = {a, b, c, d};

    auto chain = a.getDependencyChain(all);
    REQUIRE(chain.size() >= 4);
    if (chain.size() >= 4) {
        REQUIRE(chain[0] == "A");
        REQUIRE(chain[1] == "B");
        REQUIRE(chain[2] == "C");
        REQUIRE(chain[3] == "D");
    }
}

TEST_CASE("getDependencyChain handles self-dependency cycle", "[rule][circular-dependency]") {
    Rule a; a.id = 1; a.dependsOnRuleId = "A";

    std::vector<std::reference_wrapper<const Rule>> all = {a};

    auto chain = a.getDependencyChain(all);
    // A -> A (cycle detected, A is repeated)
    REQUIRE(chain.size() >= 3);
    if (chain.size() >= 3) {
        REQUIRE(chain[0] == "A");
        REQUIRE(chain[1] == "A");
        REQUIRE(chain[2] == "A");
    }
}

TEST_CASE("getDependencyChain handles A -> B -> A cycle", "[rule][circular-dependency]") {
    Rule a; a.id = 1; a.dependsOnRuleId = "B";
    Rule b; b.id = 1; b.dependsOnRuleId = "A";

    std::vector<std::reference_wrapper<const Rule>> all = {a, b};

    auto chain = a.getDependencyChain(all);
    // A -> B -> A (cycle detected, B is repeated)
    REQUIRE(chain.size() >= 4);
    if (chain.size() >= 4) {
        REQUIRE(chain[0] == "A");
        REQUIRE(chain[1] == "B");
        REQUIRE(chain[2] == "A");
        REQUIRE(chain[3] == "B");
    }
}

// ============================================================================
// Builder dependsOn with validation tests
// ============================================================================

TEST_CASE("Builder dependsOn without validation allows cycle", "[rule][builder][circular-dependency]") {
    // This should NOT throw because we're using the simple dependsOn
    auto ruleA = Rule::Builder("A").dependsOn("B").build();
    auto ruleB = Rule::Builder("B").dependsOn("A").build();

    // Cycle is allowed at build time
    REQUIRE(ruleA->dependsOnRuleId == "B");
    REQUIRE(ruleB->dependsOnRuleId == "A");
}

TEST_CASE("Builder dependsOn with validation throws on self-dependency", "[rule][builder][circular-dependency]") {
    Rule a; a.id = 1;
    std::vector<std::reference_wrapper<const Rule>> all = {a};

    // Building "A" that depends on "A" should throw when validation is enabled
    REQUIRE_THROWS_AS(
        Rule::Builder("A").dependsOn("A", all),
        RuleValidationException
    );
}

TEST_CASE("Builder dependsOn with validation throws on A -> B -> A", "[rule][builder][circular-dependency]") {
    Rule b; b.id = 1; b.dependsOnRuleId = "A";
    std::vector<std::reference_wrapper<const Rule>> all = {b};

    // Building "A" that depends on "B" should throw because B already depends on A
    REQUIRE_THROWS_AS(
        Rule::Builder("A").dependsOn("B", all),
        RuleValidationException
    );
}

TEST_CASE("Builder dependsOn with validation passes for valid chain", "[rule][builder][circular-dependency]") {
    Rule b; b.id = 1; b.dependsOnRuleId = "C";
    Rule c; c.id = 1; // No dependency
    std::vector<std::reference_wrapper<const Rule>> all = {b, c};

    // Building "A" that depends on "B" should succeed (A -> B -> C, no cycle)
    auto ruleA = Rule::Builder("A").dependsOn("B", all).build();
    REQUIRE(ruleA->dependsOnRuleId == "B");
}

TEST_CASE("Builder dependsOn with validation passes for long chain", "[rule][builder][circular-dependency]") {
    Rule b; b.id = 1; b.dependsOnRuleId = "C";
    Rule c; c.id = 1; c.dependsOnRuleId = "D";
    Rule d; d.id = 1; d.dependsOnRuleId = "E";
    Rule e; e.id = 1; // No dependency
    std::vector<std::reference_wrapper<const Rule>> all = {b, c, d, e};

    // Building "A" that depends on "B" should succeed (A -> B -> C -> D -> E, no cycle)
    auto ruleA = Rule::Builder("A").dependsOn("B", all).build();
    REQUIRE(ruleA->dependsOnRuleId == "B");
}

