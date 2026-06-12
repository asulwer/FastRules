#include "fastrules/rule.hpp"
#include "fastrules/lua_engine.hpp"
#include "fastrules/rule_context.hpp"
#include "fastrules/rule_result.hpp"
#include "fastrules/parameter_validator.hpp"
#include "fastrules/expression_validator.hpp"
#include "fastrules/rate_limiter.hpp"
#include "fastrules/performance_counters.hpp"
#include "fastrules/logger.hpp"

#include <chrono>
#include <stdexcept>
#include <algorithm>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <functional>

namespace fastrules {

// RuleResult implementation
bool RuleResult::isFullySuccessful() const noexcept {
    if (!success || exception.has_value() || skipped) return false;
    for (const auto& child : childResults) {
        if (!child.isFullySuccessful()) return false;
    }
    return true;
}

std::string Rule::buildCacheKey(const std::vector<RuleParameter>& parameters) const {
    std::ostringstream key;
    key << id << "|";
    for (const auto& param : parameters) {
        key << param.name << "=";
        // Use std::any type_info for exact type matching + value extraction
        if (param.value.has_value()) {
            const std::type_info& ti = param.value.type();
            if (ti == typeid(int)) {
                key << std::any_cast<int>(param.value);
            } else if (ti == typeid(long)) {
                key << std::any_cast<long>(param.value);
            } else if (ti == typeid(long long)) {
                key << std::any_cast<long long>(param.value);
            } else if (ti == typeid(double)) {
                key << std::any_cast<double>(param.value);
            } else if (ti == typeid(float)) {
                key << std::any_cast<float>(param.value);
            } else if (ti == typeid(bool)) {
                key << std::any_cast<bool>(param.value);
            } else if (ti == typeid(std::string)) {
                key << std::any_cast<std::string>(param.value);
            } else if (ti == typeid(const char*)) {
                key << std::any_cast<const char*>(param.value);
            } else {
                // For complex types, hash the type_info + use type name as proxy
                key << (param.type.has_value() ? param.type.value().name() : "unknown") << "@" << ti.hash_code();
            }
        } else {
            key << "nil";
        }
        key << ";";
    }
    return key.str();
}

void Rule::compile(LuaEngine& engine) {
    if (engineExpressionRefs_.count(&engine) > 0 || engineActionRefs_.count(&engine) > 0) {
        return;
    }

    if (!expression.empty()) {
        auto validation = ExpressionValidator::validate(expression);
        if (!validation.valid) {
            std::ostringstream oss;
            oss << "Rule '" << id << "': Expression validation failed\n";
            oss << "  Expression: " << expression << "\n";
            oss << "  Errors:\n";
            for (const auto& err : validation.errors) {
                oss << "    - " << err << "\n";
            }
            if (!validation.warnings.empty()) {
                oss << "  Warnings:\n";
                for (const auto& warn : validation.warnings) {
                    oss << "    - " << warn << "\n";
                }
            }
            throw RuleCompilationException(oss.str());
        }
        try {
            compiledExpressionRef = engine.compileExpression(expression);
        } catch (const std::exception& e) {
            throw RuleCompilationException(
                "Rule '" + std::to_string(id) + "': Failed to compile expression\n  Expression: " + expression + "\n  Error: " + e.what()
            );
        }
    }

    if (!action.empty()) {
        auto validation = ExpressionValidator::validate(action);
        if (!validation.valid) {
            std::ostringstream oss;
            oss << "Rule '" << std::to_string(id) << "': Action validation failed\n";
            oss << "  Action: " << action << "\n";
            oss << "  Errors:\n";
            for (const auto& err : validation.errors) {
                oss << "    - " << err << "\n";
            }
            throw RuleCompilationException(oss.str());
        }
        try {
            compiledActionRef = engine.compileAction(action);
        } catch (const std::exception& e) {
            throw RuleCompilationException(
                "Rule '" + std::to_string(id) + "': Failed to compile action\n  Action: " + action + "\n  Error: " + e.what()
            );
        }
    }

    // Compile child rules
    for (auto& child : childRules) {
        child->compile(engine);
    }

    isCompiled = true;
}

void Rule::validate(const std::vector<std::reference_wrapper<const Rule>>& allRules) {
    if (isValidated) {
        return;
    }

    // === Circular dependency detection ===
    // Build a graph of all rules and detect cycles using DFS.
    // Edges: dependsOnRuleName (horizontal) + parentRule (vertical from childRules)
    
    std::unordered_map<Id, std::vector<Id>> adjacency;
    std::unordered_map<Id, const Rule*> ruleById;
    std::unordered_map<std::string, Id> ruleByName;
    
    for (const auto& r : allRules) {
        const Rule& rule = r.get();
        ruleById[rule.id] = &rule;
        if (!rule.name.empty()) {
            ruleByName[rule.name] = rule.id;
        }
        
        // Add horizontal dependency edge (by name lookup)
        if (rule.dependsOnRuleName.has_value()) {
            auto it = ruleByName.find(rule.dependsOnRuleName.value());
            if (it != ruleByName.end()) {
                adjacency[rule.id].push_back(it->second);
            }
        }
        
        // Add vertical edges: child -> parent (childRules creates parent -> child
        // execution dependency, so add parent depends on child completing first)
        // Actually child rules are executed BEFORE parent, so parent depends on child
        for (const auto& child : rule.childRules) {
            if (child && child->id != 0) {
                adjacency[rule.id].push_back(child->id);
                ruleById[child->id] = child.get();
            }
        }
    }
    
    // DFS cycle detection
    enum class Color { White, Gray, Black };
    std::unordered_map<Id, Color> color;
    std::vector<Id> path;
    
    for (const auto& [nodeId, _] : ruleById) {
        color[nodeId] = Color::White;
    }
    
    std::function<void(const Id&, std::vector<Id>&)> dfs = [&](const Id& node, std::vector<Id>& stack) {
        color[node] = Color::Gray;
        stack.push_back(node);
        
        for (const Id& neighbor : adjacency[node]) {
            auto it = color.find(neighbor);
            if (it == color.end()) {
                // Neighbor not in allRules - skip (existence check handles this separately)
                continue;
            }
            
            if (it->second == Color::Gray) {
                // Found cycle - build error message
                auto cycleStart = std::find(stack.begin(), stack.end(), neighbor);
                std::ostringstream oss;
                oss << "Circular dependency detected: ";
                for (auto it2 = cycleStart; it2 != stack.end(); ++it2) {
                    if (it2 != cycleStart) oss << " → ";
                    oss << *it2;
                }
                oss << " → " << neighbor;
                throw RuleValidationException(oss.str());
            }
            
            if (it->second == Color::White) {
                dfs(neighbor, stack);
            }
        }
        
        stack.pop_back();
        color[node] = Color::Black;
    };
    
    for (const auto& [nodeId, _] : ruleById) {
        if (color[nodeId] == Color::White) {
            std::vector<Id> stack;
            dfs(nodeId, stack);
        }
    }

    // === Empty rule ID check ===
    for (const auto& r : allRules) {
        if (r.get().id == 0) {
            throw RuleValidationException("Rule has zero ID");
        }
    }

    // === Duplicate rule ID check ===
    std::unordered_set<Id> seenIds;
    for (const auto& r : allRules) {
        const Id& rid = r.get().id;
        if (seenIds.count(rid)) {
            throw RuleValidationException("Duplicate rule ID: " + rid);
        }
        seenIds.insert(rid);
    }

    // === Dependency existence check ===
    if (dependsOnRuleName.has_value()) {
        bool found = false;
        for (const auto& r : allRules) {
            if (r.get().name == dependsOnRuleName.value()) {
                found = true;
                break;
            }
        }
        if (!found) {
            throw RuleValidationException("Rule depends on non-existent rule: " + dependsOnRuleName.value());
        }
    }

    isValidated = true;
}

bool Rule::hasCircularDependency(const std::vector<std::reference_wrapper<const Rule>>& allRules) const {
    if (!dependsOnRuleName.has_value()) {
        return false;
    }

    // Build lookup maps from allRules
    std::unordered_map<int, const Rule*> ruleMap;
    std::unordered_map<std::string, int> nameToId;
    for (const auto& ref : allRules) {
        ruleMap[ref.get().id] = &ref.get();
        if (!ref.get().name.empty()) {
            nameToId[ref.get().name] = ref.get().id;
        }
    }

    // Follow the dependency chain
    std::unordered_set<int> visited;
    auto it = nameToId.find(dependsOnRuleName.value());
    if (it == nameToId.end()) {
        return false; // Dependency not found
    }
    int current = it->second;

    while (current != 0) {
        // Check if we've returned to the starting rule
        if (current == id) {
            return true;
        }

        // Check if we've visited this rule before (cycle between other rules)
        if (visited.count(current)) {
            return true;
        }
        visited.insert(current);

        // Move to next dependency
        auto depIt = ruleMap.find(current);
        if (depIt == ruleMap.end() || !depIt->second->dependsOnRuleName.has_value()) {
            break;
        }
        auto nameIt = nameToId.find(depIt->second->dependsOnRuleName.value());
        if (nameIt == nameToId.end()) {
            break;
        }
        current = nameIt->second;
    }

    return false;
}

std::vector<Rule::Id> Rule::getDependencyChain(const std::vector<std::reference_wrapper<const Rule>>& allRules) const {
    std::vector<Rule::Id> chain;
    chain.push_back(id);

    if (!dependsOnRuleName.has_value()) {
        return chain;
    }

    // Build lookup map from allRules
    std::unordered_map<int, const Rule*> ruleMap;
    std::unordered_map<std::string, int> nameToId;
    for (const auto& ref : allRules) {
        ruleMap[ref.get().id] = &ref.get();
        if (!ref.get().name.empty()) {
            nameToId[ref.get().name] = ref.get().id;
        }
    }

    // Follow the dependency chain
    std::unordered_set<int> visited;
    auto nameIt = nameToId.find(dependsOnRuleName.value());
    if (nameIt == nameToId.end()) {
        return chain;
    }
    int current = nameIt->second;

    while (current != 0) {
        chain.push_back(current);

        // Check for cycle
        if (visited.count(current)) {
            // Cycle detected - stop here
            break;
        }
        visited.insert(current);

        // Move to next dependency
        auto ruleIt = ruleMap.find(current);
        if (ruleIt == ruleMap.end() || !ruleIt->second->dependsOnRuleName.has_value()) {
            break;
        }
        auto nextNameIt = nameToId.find(ruleIt->second->dependsOnRuleName.value());
        if (nextNameIt == nameToId.end()) {
            break;
        }
        current = nextNameIt->second;
    }

    return chain;
}

void Rule::storeInCache(const std::vector<RuleParameter>& parameters, const RuleResult& result) const {
    if (cacheDuration.has_value() && cacheDuration->count() > 0) {
        std::lock_guard<std::mutex> lock(*cacheMutex_);
        auto cacheKey = buildCacheKey(parameters);
        cache_[cacheKey] = {
            std::make_shared<RuleResult>(result), 
            std::chrono::steady_clock::now() + cacheDuration.value(),
            cacheGeneration_
        };
    }
}

void Rule::setFailure(RuleResult& result, const std::string& message, bool wasCached) noexcept {
    result.success = false;
    result.exception = RuleException(message);
    if (!result.skipped) {
        PerformanceCounters::instance().recordExecution(false, false, wasCached, false, false);
    } else {
        PerformanceCounters::instance().recordExecution(false, true, false, false, false);
    }
}

RuleResult Rule::execute(LuaEngine& engine, RuleContext& context, const std::vector<RuleParameter>& parameters) {
    auto log = fastrules::logger();
    log->trace("Executing rule {}", id);

    // Check cache first if cacheDuration is set
    if (cacheDuration.has_value() && cacheDuration->count() > 0) {
        std::lock_guard<std::mutex> lock(*cacheMutex_);
        auto cacheKey = buildCacheKey(parameters);
        auto it = cache_.find(cacheKey);
        if (it != cache_.end()) {
            // Check if cache entry is still valid (not expired and same generation)
            if (std::chrono::steady_clock::now() < it->second.expiresAt && 
                it->second.generation == cacheGeneration_) {
                // Cache hit - return cached result
                log->debug("Cache hit for rule {}", id);
                PerformanceCounters::instance().recordExecution(it->second.result->isSuccess(), false, true, false, false);
                return *it->second.result;
            }
            // Cache expired or invalidated - remove it
            log->debug("Cache expired or invalidated for rule {}", id);
            cache_.erase(it);
        }
    }

    RuleResult result;
    result.ruleName = name;  // Use human-readable name instead of ID
    result.executedAt = std::chrono::steady_clock::now();

    // Preference: inactive rules are completely skipped - no result, no evaluation
    if (!isActive) {
        log->debug("Rule {} inactive — skipped", id);
        result.success = false;
        result.skipped = true;
        return result;
    }

    auto startTime = std::chrono::steady_clock::now();

    try {
        // Check rate limiting BEFORE parameter validation (cheaper fail-fast)
        RateLimiter* limiter = nullptr;
        if (rateLimiter.has_value() && rateLimiter.value()) {
            limiter = rateLimiter.value().get();
        } else {
            limiter = &RateLimiter::global();
        }
        if (!limiter->isAllowed(std::to_string(id))) {
            log->warn("Rate limit exceeded for rule {}", id);
            throw RateLimitException("Rate limit exceeded for rule '" + std::to_string(id) + "'");
        }

        // Validate parameter types before execution
        ParameterValidator::validateTypes(parameters);


        // Step 1: Execute child rules (bottom-up / bubble-up)
        if (!childRules.empty()) {
            log->debug("Executing {} child rules for rule {}", childRules.size(), id);
            result.childResults = executeChildRules(engine, context, parameters);

            // Store child results in context so parent expressions can access them
            for (const auto& childResult : result.childResults) {
                context.setResult(childResult.ruleName, childResult);
            }

            // Preference: parent only evaluates if ALL children pass
            for (const auto& childResult : result.childResults) {
                if (!childResult.isSuccess()) {
                    log->info("Child rule {} failed — parent {} aborted", childResult.ruleName, id);
                    setFailure(result, "Child rule " + std::to_string(childResult.ruleName) + " failed");
                    storeInCache(parameters, result);
                    context.setResult(name, result);
                    return result;
                }
            }
        }

        // Step 2: Evaluate parent expression (only if all children passed)
        if (!expression.empty() && compiledExpressionRef.has_value()) {
            log->trace("Evaluating expression for rule {}: {}", id, expression);
            bool exprResult = evaluateExpression(engine, context, parameters);
            if (!exprResult) {
                log->info("Rule {} expression evaluated to false", id);
                setFailure(result, "Expression evaluated to false");
                storeInCache(parameters, result);
                context.setResult(name, result);
                return result;
            }
        }

        // Step 3: Execute action (only if expression passed)
        if (!action.empty() && compiledActionRef.has_value()) {
            log->trace("Executing action for rule {}: {}", id, action);
            executeAction(engine, context, parameters);
        }

        result.success = true;
        log->debug("Rule {} executed successfully", id);
        // Record successful execution
        PerformanceCounters::instance().recordExecution(true, false, false, false, false);

    } catch (const RateLimitException& ex) {
        log->error("Rate limit exception in rule {}: {}", id, ex.what());
        result.success = false;
        result.exception = RuleException(ex.what());
        context.setLastError(id, "Rate limit exceeded: " + std::string(ex.what()));
        PerformanceCounters::instance().recordExecution(false, false, false, false, true);
    } catch (const RuleTimeoutException& ex) {
        log->error("Timeout in rule {}: {}", id, ex.what());
        result.success = false;
        result.exception = ex;
        context.setLastError(id, "Timeout: " + std::string(ex.what()));
        PerformanceCounters::instance().recordExecution(false, false, false, true, false);
    } catch (const RuleException& ex) {
        log->error("Rule {} exception: {}", id, ex.what());
        setFailure(result, ex.what());
        context.setLastError(name, ex.what());
    } catch (const std::exception& ex) {
        log->error("Standard exception in rule {}: {}", id, ex.what());
        setFailure(result, ex.what());
        context.setLastError(name, ex.what());
    } catch (...) {
        log->critical("Unknown exception during rule {} execution", id);
        setFailure(result, "Unknown exception during rule execution");
        context.setLastError(name, "Unknown exception");
    }

    auto endTime = std::chrono::steady_clock::now();
    result.metrics.evaluationCount = 1;
    result.metrics.totalExecutionTime = endTime - startTime;
    if (!result.success) {
        result.metrics.failureCount = 1;
    }
    result.metrics.lastExecuted = endTime;

    // Record performance metrics - only execution time here
    // Success/failure/skipped are already recorded in setFailure() or the success path
    PerformanceCounters::instance().recordExecutionTime(
        std::chrono::duration_cast<std::chrono::nanoseconds>(endTime - startTime));

    // Store result in context for dependency access
    context.setResult(name, result);

    // Store in cache if applicable
    storeInCache(parameters, result);

    return std::move(result);
}

std::vector<RuleResult> Rule::executeChildRules(LuaEngine& engine, RuleContext& context, const std::vector<RuleParameter>& parameters) {
    std::vector<RuleResult> results;
    results.reserve(childRules.size());

    for (auto& child : childRules) {
        results.emplace_back(child->execute(engine, context, parameters));
    }

    return results;
}

bool Rule::evaluateExpression(LuaEngine& engine, RuleContext& context, const std::vector<RuleParameter>& parameters) {
    auto ref = getExpressionRef(engine);
    if (ref == -1) {
        return true; // No expression = pass
    }
    return engine.evaluateExpression(ref, parameters, context, timeout);
}

void Rule::executeAction(LuaEngine& engine, RuleContext& context, const std::vector<RuleParameter>& parameters) {
    auto ref = getActionRef(engine);
    if (ref == -1) {
        return;
    }
    engine.executeAction(ref, parameters, context, timeout);
}

int Rule::getExpressionRef(LuaEngine& engine) {
    auto it = engineExpressionRefs_.find(&engine);
    if (it != engineExpressionRefs_.end()) {
        return it->second;
    }
    // Fallback to the default compiled ref
    if (compiledExpressionRef.has_value()) {
        return compiledExpressionRef.value();
    }
    return -1;
}

int Rule::getActionRef(LuaEngine& engine) {
    auto it = engineActionRefs_.find(&engine);
    if (it != engineActionRefs_.end()) {
        return it->second;
    }
    // Fallback to the default compiled ref
    if (compiledActionRef.has_value()) {
        return compiledActionRef.value();
    }
    return -1;
}

// Static factory methods for predicates
Rule Rule::isNotNull(const std::string& parameterName, const std::string& description) {
    Rule rule;
    rule.description = description.empty() ? parameterName + " is not null" : description;
    rule.expression = "isNotNull(" + parameterName + ")";
    return rule;
}

Rule Rule::greaterThan(const std::string& parameterName, double value, const std::string& description) {
    Rule rule;
    rule.description = description.empty() ? parameterName + " > " + std::to_string(value) : description;
    rule.expression = parameterName + " > " + std::to_string(value);
    return rule;
}

Rule Rule::lessThan(const std::string& parameterName, double value, const std::string& description) {
    Rule rule;
    rule.description = description.empty() ? parameterName + " < " + std::to_string(value) : description;
    rule.expression = parameterName + " < " + std::to_string(value);
    return rule;
}

Rule Rule::equals(const std::string& parameterName, const std::string& value, const std::string& description) {
    Rule rule;
    rule.description = description.empty() ? parameterName + " == " + value : description;
    rule.expression = parameterName + " == \"" + value + "\"";
    return rule;
}

Rule Rule::matchesRegex(const std::string& parameterName, const std::string& pattern, const std::string& description) {
    Rule rule;
    rule.description = description.empty() ? parameterName + " matches " + pattern : description;
    rule.expression = "matchesRegex(" + parameterName + ", \"" + pattern + "\")";
    return rule;
}

Rule Rule::contains(const std::string& parameterName, const std::string& substring, const std::string& description) {
    Rule rule;
    rule.description = description.empty() ? parameterName + " contains " + substring : description;
    rule.expression = "string.find(" + parameterName + ", \"" + substring + "\") ~= nil";
    return rule;
}

// ============================================================================
// Builder factory
// ============================================================================

Rule::Builder Rule::create(const Id& id, const std::string& expression, bool active) {
    return Builder(id)
        .withExpression(expression)
        .active(active);
}

// ============================================================================
// Cache invalidation
// ============================================================================

int Rule::invalidateCache() {
    std::lock_guard<std::mutex> lock(*cacheMutex_);
    ++cacheGeneration_;
    cache_.clear();  // Clear all cached entries
    return cacheGeneration_;
}

} // namespace fastrules
