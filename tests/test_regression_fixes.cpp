/**
 * @file test_regression_fixes.cpp
 * @brief Regression tests for previously-shipped defects.
 *
 * Each test here reproduces a concrete bug that existed in the codebase and
 * was not covered by any other test. They are grouped in one file so the
 * originating defect stays attached to the assertion.
 */

#include <doctest/doctest.h>

#include <fastrules.hpp>
#include <fastrules/async_workflow.hpp>
#include <fastrules/expression_validator.hpp>
#include <fastrules/memory_pool.hpp>
#include <fastrules/rule_versioning.hpp>

#include <string>
#include <type_traits>
#include <utility>
#include <vector>

using namespace fastrules;

// ============================================================================
// Compiled references are per-engine
//
// Defect: Rule cached a single compiledExpressionRef shared across every
// engine, so a second workflow compiled into the same engine evaluated the
// FIRST workflow's expression - silently returning the wrong answer.
// ============================================================================

TEST_CASE("Two workflows sharing one engine evaluate their own expressions") {
    LuaEngine engine;

    Workflow wf1;
    wf1.id = 1;
    wf1.rules.push_back(Rule::Builder(1).withName("a1").withExpression("x > 100").build());
    wf1.compile(engine);

    Workflow wf2;
    wf2.id = 2;
    wf2.rules.push_back(Rule::Builder(2).withName("b1").withExpression("x < 100").build());
    wf2.compile(engine);

    std::vector<RuleParameter> params;
    params.emplace_back("x", 5);

    auto r1 = wf1.execute(engine, params);
    auto r2 = wf2.execute(engine, params);

    REQUIRE(r1.size() == 1);
    REQUIRE(r2.size() == 1);
    CHECK(r1[0].isSuccess() == false);  // 5 > 100 is false
    CHECK(r2[0].isSuccess() == true);   // 5 < 100 is true
}

TEST_CASE("A rule executes correctly on an engine it was not pre-compiled for") {
    LuaEngine compileEngine;
    LuaEngine otherEngine;

    auto rule = Rule::Builder(1).withName("r").withExpression("x < 10").build();
    rule->compile(compileEngine);

    // Give the other engine a different ref numbering so a stale ref would
    // resolve to the wrong chunk rather than simply failing.
    auto filler = otherEngine.compileExpression("1 == 1");
    REQUIRE(filler.has_value());

    RuleContext ctx;
    std::vector<RuleParameter> params;
    params.emplace_back("x", 5);

    auto result = rule->execute(otherEngine, ctx, params);
    CHECK(result.isSuccess() == true);
}

TEST_CASE("Editing a rule's expression and recompiling takes effect") {
    LuaEngine engine;
    RuleContext ctx;
    std::vector<RuleParameter> params;

    auto rule = Rule::Builder(1).withName("r").withExpression("true").build();
    rule->compile(engine);
    CHECK(rule->execute(engine, ctx, params).isSuccess() == true);

    rule->expression = "false";
    rule->compile(engine);
    CHECK(rule->execute(engine, ctx, params).isSuccess() == false);
}

// ============================================================================
// Expression validator matches on identifier boundaries
//
// Defect: plain substring matching rejected any expression containing "load",
// "package" or "module" - so "payload > 10" could not be compiled at all.
// ============================================================================

TEST_CASE("Validator accepts identifiers that merely contain a dangerous word") {
    CHECK(ExpressionValidator::validate("payload > 10").valid);
    CHECK(ExpressionValidator::validate("download_count < 5").valid);
    CHECK(ExpressionValidator::validate("upload_size >= 1").valid);
    CHECK(ExpressionValidator::validate("package_type == \"box\"").valid);
    CHECK(ExpressionValidator::validate("module_id ~= 3").valid);

    LuaEngine engine;
    auto rule = Rule::Builder(1).withName("r").withExpression("payload > 10").build();
    CHECK_NOTHROW(rule->compile(engine));
}

TEST_CASE("Validator still rejects genuine dangerous calls") {
    CHECK_FALSE(ExpressionValidator::validate("os.execute('ls')").valid);
    CHECK_FALSE(ExpressionValidator::validate("load('code')").valid);
    CHECK_FALSE(ExpressionValidator::validate("dofile('x.lua')").valid);
    CHECK_FALSE(ExpressionValidator::validate("io.open('/etc/passwd')").valid);
}

// ============================================================================
// Rule::validate duplicate-id message
//
// Defect: built via `"literal" + id`, i.e. pointer arithmetic on a string
// literal, producing an out-of-bounds read and a garbage message.
// ============================================================================

TEST_CASE("Duplicate rule ID reports the id, not garbage") {
    auto a = Rule::Builder(4242).withName("a").withExpression("true").build();
    auto b = Rule::Builder(4242).withName("b").withExpression("true").build();
    std::vector<std::reference_wrapper<const Rule>> all{*a, *b};

    REQUIRE_THROWS_AS(a->validate(all), RuleValidationException);
    try {
        a->validate(all);
    } catch (const RuleValidationException& e) {
        std::string msg = e.what();
        CHECK(msg.find("4242") != std::string::npos);
    }
}

// ============================================================================
// AsyncTask coroutine lifetime
//
// Defect: final_suspend() returned suspend_never, so the frame self-destructed
// at co_return; get() then read freed memory and ~AsyncTask double-destroyed
// the frame (observed as a segfault).
// ============================================================================

namespace {
struct LifetimeProbe {
    static int liveCount;
    int value = 0;
    LifetimeProbe() { ++liveCount; }
    LifetimeProbe(const LifetimeProbe& o) : value(o.value) { ++liveCount; }
    LifetimeProbe& operator=(const LifetimeProbe& o) { value = o.value; return *this; }
    ~LifetimeProbe() { --liveCount; }
};
int LifetimeProbe::liveCount = 0;

fastrules::AsyncTask<LifetimeProbe> makeProbeTask(int v) {
    LifetimeProbe p;
    p.value = v;
    co_return p;
}
}  // namespace

TEST_CASE("AsyncTask keeps its coroutine frame alive until destroyed") {
    LifetimeProbe::liveCount = 0;
    {
        auto task = makeProbeTask(42);
        // The promise still owns its value here; with suspend_never the frame
        // (and the value) would already be gone.
        CHECK(LifetimeProbe::liveCount > 0);
        CHECK(task.get().value == 42);
    }
    // Destroying the task destroys the frame exactly once.
    CHECK(LifetimeProbe::liveCount == 0);
}

// ============================================================================
// RuleVersionManager locking
//
// Defect: snapshotWorkflow() held mutex_ and then called snapshotRule(), which
// re-locked the same non-recursive mutex - throwing on MSVC and deadlocking on
// libstdc++/libc++.
// ============================================================================

TEST_CASE("snapshotWorkflow does not re-lock its own mutex") {
    Workflow wf;
    wf.id = 1;
    wf.rules.push_back(Rule::Builder(1).withName("r1").withExpression("true").build());
    wf.rules.push_back(Rule::Builder(2).withName("r2").withExpression("true").build());

    RuleVersionManager mgr;
    REQUIRE_NOTHROW(mgr.snapshotWorkflow(wf, "author", "initial"));
    CHECK(mgr.getTrackedRuleIds().size() == 2);

    auto history = mgr.getHistory(1);
    REQUIRE(history.has_value());
    CHECK(history->getVersions().size() == 1);
}

// ============================================================================
// Parallel compilation
//
// Defect: buildCompilationLevels() counted child rules as in-edges on their
// parent but never decremented them, so any workflow of >=10 rules containing
// a rule with children threw "Not all rules were compiled".
// ============================================================================

TEST_CASE("compileParallel handles rules that own child rules") {
    LuaEngine engine;
    Workflow wf;
    wf.id = 1;

    for (int i = 1; i <= 12; ++i) {
        auto builder = Rule::Builder(i)
                           .withName("r" + std::to_string(i))
                           .withExpression("x > 0");
        if (i == 12) {
            builder.withChild(Rule::Builder(100).withName("c1").withExpression("x > 0").build());
        }
        wf.rules.push_back(builder.build());
    }

    REQUIRE_NOTHROW(wf.compileParallel(engine, 4));
    CHECK(wf.isCompiled());

    std::vector<RuleParameter> params;
    params.emplace_back("x", 5);
    auto results = wf.execute(engine, params);
    CHECK(results.size() == 12);
    for (const auto& r : results) {
        CHECK(r.isSuccess());
    }
}

// ============================================================================
// Context keys for unnamed rules
//
// Defect: every unnamed rule stored its result under the empty-string key, so
// they all overwrote one another.
// ============================================================================

TEST_CASE("Unnamed rules do not share a single context key") {
    LuaEngine engine;
    RuleContext ctx;

    auto passing = Rule::Builder(1).withExpression("true").build();
    auto failing = Rule::Builder(2).withExpression("false").build();
    passing->compile(engine);
    failing->compile(engine);

    std::vector<RuleParameter> params;
    passing->execute(engine, ctx, params);
    failing->execute(engine, ctx, params);

    auto first = ctx.getResult(passing->resultKey());
    auto second = ctx.getResult(failing->resultKey());

    REQUIRE(first.has_value());
    REQUIRE(second.has_value());
    CHECK(passing->resultKey() != failing->resultKey());
    CHECK(first->isSuccess() == true);
    CHECK(second->isSuccess() == false);
}

// ============================================================================
// MemoryPool accounting
//
// Defect: release() decremented an unsigned allocatedCount_ even for objects
// that never incremented it, wrapping the counter to SIZE_MAX.
// ============================================================================

TEST_CASE("MemoryPool allocation counter never underflows") {
    MemoryPool<std::string> pool(1);  // capacity of one

    auto a = pool.acquire();
    auto b = pool.acquire();
    auto c = pool.acquire();

    pool.release(std::move(a));  // fills the pool
    pool.release(std::move(b));  // over capacity -> destroyed
    pool.release(std::move(c));  // over capacity -> destroyed

    auto stats = pool.getStats();
    CHECK(stats.second < 1000);  // would be ~SIZE_MAX if it had wrapped
}

// ============================================================================
// coExecuteWorkflow ownership
//
// Defect: it took Workflow& and moved from it, silently gutting an object the
// caller still owned. It now takes the workflow by value, so the transfer is
// explicit at the call site.
// ============================================================================

TEST_CASE("coExecuteWorkflow takes ownership of the workflow") {
    static_assert(
        std::is_invocable_r_v<AsyncWorkflowTask, decltype(&coExecuteWorkflow),
                              Workflow, LuaEngine&, const std::vector<RuleParameter>&, size_t>,
        "coExecuteWorkflow should accept a Workflow by value");

    LuaEngine engine;
    Workflow wf;
    wf.id = 1;
    wf.rules.push_back(Rule::Builder(1).withName("r1").withExpression("x > 0").build());
    wf.rules.push_back(Rule::Builder(2).withName("r2").withExpression("x < 100").build());

    std::vector<RuleParameter> params;
    params.emplace_back("x", 5);

    auto task = coExecuteWorkflow(std::move(wf), engine, params, 2);
    auto results = task.get();

    CHECK(results.size() == 2);
    for (const auto& r : results) {
        CHECK(r.isSuccess());
    }
}

// ============================================================================
// Parallel execution with more rules than pooled engines
// ============================================================================

TEST_CASE("executeParallel completes when rules outnumber pooled engines") {
    LuaEngine engine;
    Workflow wf;
    wf.id = 1;

    const int kRules = 64;
    for (int i = 1; i <= kRules; ++i) {
        wf.rules.push_back(Rule::Builder(i)
                               .withName("p" + std::to_string(i))
                               .withExpression("x > 0")
                               .build());
    }
    wf.compile(engine);

    std::vector<RuleParameter> params;
    params.emplace_back("x", 5);

    auto results = wf.executeParallel(engine, params);
    CHECK(results.size() == static_cast<size_t>(kRules));
    for (const auto& r : results) {
        CHECK(r.isSuccess());
    }
}
