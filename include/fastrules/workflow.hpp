/**
 * @file workflow.hpp
 * @brief Workflow orchestration - ordered rule execution with dependencies
 * 
 * Workflow manages a collection of rules and their execution order.
 * Key responsibilities:
 * - Dependency resolution (topological sort)
 * - Rule compilation orchestration
 * - Sequential, parallel, and adaptive execution
 * - Engine pool management for concurrent execution
 * 
 * Execution Modes:
 * 1. Sequential (execute): Rules execute one at a time in dependency order
 * 2. Parallel (executeParallel): Independent rules execute concurrently
 * 3. Adaptive (executeAdaptive): Automatically chooses based on rule count
 * 4. Traced (executeWithTrace): Captures detailed execution trace
 * 5. Async (executeAsync): Returns future for non-blocking execution
 * 6. Streaming (executeStreaming): Generator-style result streaming
 * 
 * Thread Safety:
 * - Construction: NOT thread-safe
 * - Compilation: NOT thread-safe (compile once)
 * - Sequential execution: Thread-safe with external synchronization
 * - Parallel execution: Thread-safe (uses engine pool)
 * 
 * The Workflow class pre-creates a pool of cloned LuaEngines during
 * compilation to enable lock-free parallel execution.
 * 
 * Example:
 * @code
 * fastrules::LuaEngine engine;
 * fastrules::Workflow workflow;
 * workflow.id = 1;
 * 
 * // Add rules
 * auto rule1 = std::make_shared<fastrules::Rule>();
 * rule1->id = 1;
 * rule1->expression = "true";
 * workflow.rules.push_back(rule1);
 * 
 * // Compile and execute
 * workflow.compile(engine);
 * auto results = workflow.execute(engine, params);
 * 
 * // Parallel execution
 * auto parResults = workflow.executeParallel(engine, params);
 * @endcode
 */

#pragma once

#include "fastrules/rule.hpp"
#include "fastrules/rule_result.hpp"
#include "fastrules/engine_pool.hpp"
#include "fastrules/execution_tracer.hpp"
#include "fastrules/streaming_result.hpp"
#include "fastrules/lua_engine.hpp"

#include <memory>
#include <vector>
#include <future>
#include <optional>
#include <chrono>

namespace fastrules {

// Forward declarations
class LuaEngine;
class RuleContext;

/**
 * @brief Container for orchestrating rule execution
 * 
 * A Workflow manages a collection of rules and provides:
 * - Dependency resolution (topological sort by dependencies)
 * - Priority ordering within dependency levels
 * - Multiple execution strategies (sequential, parallel, adaptive)
 * - Engine pool management for efficient parallel execution
 * - Compilation orchestration (compiles all rules into Lua)
 * 
 * Dependencies:
 * Rules can declare dependencies on other rules by name. The workflow
 * uses Kahn's algorithm for topological sorting, respecting dependencies
 * while maximizing parallelism.
 * 
 * Circular Dependencies:
 * The workflow detects circular dependencies during validation and
 * throws RuleValidationException if found.
 * 
 * Inactive Rules:
 * Inactive rules are skipped during execution without producing results.
 */
class Workflow {
public:
    // ========================================================================
    // Configuration
    // ========================================================================
    
    int id = 0;                            ///< Workflow identifier
    std::string description;               ///< Human-readable description
    bool isActive = true;                  ///< Whether this workflow is active

    // ========================================================================
    // Rules
    // ========================================================================
    
    /**
     * @brief The rules in this workflow
     * 
     * Rules are stored as shared_ptr to allow sharing between workflows
     * and enable child rules to reference parents.
     * 
     * Rules execute in dependency order, not insertion order.
     * Use resolveExecutionOrder() to see the actual execution order.
     */
    std::vector<std::shared_ptr<Rule>> rules;

    // ========================================================================
    // Construction
    // ========================================================================
    
    /// @brief Default constructor
    Workflow() = default;
    
    /// @brief Virtual destructor
    virtual ~Workflow();
    
    /// @brief Move constructor
    Workflow(Workflow&&) = default;
    
    /// @brief Move assignment
    Workflow& operator=(Workflow&&) = default;
    
    /// @brief Disable copy (rules contain unique state)
    Workflow(const Workflow&) = delete;
    
    /// @brief Disable copy assignment
    Workflow& operator=(const Workflow&) = delete;

    // ========================================================================
    // Validation
    // ========================================================================
    
    /**
     * @brief Validate the workflow before compilation
     * 
     * Performs validation checks:
     * - Duplicate rule ID detection
     * - Circular dependency detection (DFS)
     * - Dependency existence verification
     * 
     * Called automatically by compile() if not already validated.
     * 
     * @throws RuleValidationException if validation fails
     */
    void validate();

    /// @brief Check if the workflow has been validated
    [[nodiscard]] bool isValidated() const noexcept { return validated_; }

    // ========================================================================
    // Compilation
    // ========================================================================
    
    /**
     * @brief Compile all rules in the workflow
     * 
     * Steps:
     * 1. Validate the workflow (if not already validated)
     * 2. Bind types and actions to the Lua engine
     * 3. Auto-discover callbacks from actions
     * 4. Compile each rule into the engine
     * 5. Create engine clones for parallel execution
     * 6. Initialize the engine pool
     * 
     * @param engine The LuaEngine to compile against
     * 
     * Thread Safety: NOT thread-safe. Call once from a single thread.
     * 
     * Performance: Compilation can be expensive. Do it once and cache
     * the workflow for repeated executions.
     */
    void compile(LuaEngine& engine);

    /// @brief Check if the workflow has been compiled
    [[nodiscard]] bool isCompiled() const noexcept;

    /**
     * @brief Compile all rules in parallel using multiple threads
     * 
     * Analyzes the rule dependency graph and compiles independent rules
     * concurrently. This is beneficial when compiling workflows with 10+
     * rules that have no interdependencies.
     * 
     * Thread Safety:
     * - NOT thread-safe - compile once from a single thread
     * - Each compilation thread gets its own LuaEngine clone
     * 
     * Performance:
     * - Best for workflows with 10+ independent rules
     * - Compilation time reduced by up to Nx on N-core systems
     * - Slight overhead from thread management for small workflows
     * 
     * Example:
     * @code
     * // Compile 20 independent rules in parallel
     * workflow.compileParallel(engine, 4);  // Use 4 threads
     * @endcode
     * 
     * @param engine The primary LuaEngine (used for binding types/actions)
     * @param numThreads Number of compilation threads (default: hardware concurrency)
     */
    void compileParallel(LuaEngine& engine, size_t numThreads = 0);

    // ========================================================================
    // Execution - Sequential
    // ========================================================================
    
    /**
     * @brief Execute rules sequentially in dependency order
     * 
     * Rules execute one at a time, respecting dependencies and priority.
     * This is the simplest execution mode with minimal overhead.
     * 
     * @param engine The LuaEngine to execute with
     * @param parameters Parameters to pass to rules
     * @return Vector of RuleResult (one per active rule executed)
     * 
     * Thread Safety: Thread-safe if engine is not shared with other threads.
     */
    std::vector<RuleResult> execute(LuaEngine& engine, 
                                      const std::vector<RuleParameter>& parameters);

    // ========================================================================
    // Execution - Parallel
    // ========================================================================
    
    /**
     * @brief Execute rules in parallel using the engine pool
     * 
     * Uses the pre-created engine pool to execute rules concurrently.
     * Rules at the same dependency level can execute in parallel.
     * Dependencies are respected - rules wait for their dependencies.
     * 
     * Thread Safety: Fully thread-safe. Uses engine pool internally.
     * 
     * @param engine The master LuaEngine (used for pool initialization)
     * @param parameters Parameters to pass to rules
     * @return Vector of RuleResult (order may not match execution order)
     */
    std::vector<RuleResult> executeParallel(LuaEngine& engine, 
                                               const std::vector<RuleParameter>& parameters);

    // ========================================================================
    // Execution - Traced
    // ========================================================================
    
    /**
     * @brief Execute with detailed tracing for debugging
     * 
     * Captures a detailed execution trace including:
     * - Each rule's start/end time
     * - Expression and action strings
     * - Dependency check results
     * - Success/failure status per stage
     * 
     * @param engine The LuaEngine
     * @param parameters Parameters to pass
     * @param tracer The tracer to record to (must be started)
     * @return Vector of RuleResult
     * 
     * Example:
     * @code
     * ExecutionTracer tracer(workflow.id);
     * tracer.start();
     * auto results = workflow.executeWithTrace(engine, params, tracer);
     * tracer.finish(true);
     * 
     * for (const auto& step : tracer.getTrace().steps) {
     *     std::cout << step.ruleName << ": " << step.stage 
     *               << " took " << step.duration().count() << "ns\n";
     * }
     * @endcode
     */
    std::vector<RuleResult> executeWithTrace(LuaEngine& engine,
                                               const std::vector<RuleParameter>& parameters,
                                               ExecutionTracer& tracer);

    // ========================================================================
    // Execution - Async
    // ========================================================================
    
    /**
     * @brief Execute asynchronously returning a future
     * 
     * Launches workflow execution on a separate thread and returns
     * a future that will contain the results.
     * 
     * @param engine The LuaEngine
     * @param parameters Parameters to pass
     * @return Future containing the results vector
     * 
     * Example:
     * @code
     * auto future = workflow.executeAsync(engine, params);
     * // Do other work...
     * auto results = future.get();  // Blocks until complete
     * @endcode
     */
    std::future<std::vector<RuleResult>> executeAsync(LuaEngine& engine,
                                                        const std::vector<RuleParameter>& parameters);

    // ========================================================================
    // Execution - Adaptive
    // ========================================================================
    
    /**
     * @brief Automatically choose sequential vs parallel execution
     * 
     * Uses a configurable threshold to decide:
     * - If rules.size() <= threshold: use sequential execution
     * - If rules.size() > threshold: use parallel execution
     * 
     * Default threshold is 4, which can be changed via setAdaptiveThreshold().
     * 
     * @param engine The LuaEngine
     * @param parameters Parameters to pass
     * @return Vector of RuleResult
     */
    std::vector<RuleResult> executeAdaptive(LuaEngine& engine,
                                               const std::vector<RuleParameter>& parameters);

    /**
     * @brief Get the current adaptive threshold
     * @return The threshold value (default: 4)
     */
    [[nodiscard]] size_t getAdaptiveThreshold() const noexcept { return adaptiveThreshold_; }

    /**
     * @brief Set the adaptive threshold
     * @param threshold New threshold value
     */
    void setAdaptiveThreshold(size_t threshold) { adaptiveThreshold_ = threshold; }

    /**
     * @brief Enable/disable auto-detection of optimal threshold
     * 
     * When enabled, the workflow periodically samples both sequential
     * and parallel execution times to find the optimal threshold.
     * 
     * @param enabled Whether to enable auto-detection
     */
    void enableAutoDetection(bool enabled) { autoDetectThreshold_ = enabled; }

    /// @brief Check if auto-detection is enabled
    [[nodiscard]] bool isAutoDetectionEnabled() const noexcept { return autoDetectThreshold_; }

    // ========================================================================
    // Execution - Streaming
    // ========================================================================
    
    /**
     * @brief Execute rules as a streaming generator
     * 
     * Returns a StreamingResult that yields results one at a time.
     * Useful for memory-constrained environments or when processing
     * results incrementally.
     * 
     * @param engine The LuaEngine
     * @param parameters Parameters to pass
     * @return StreamingResult generator
     * 
     * Example:
     * @code
     * auto stream = workflow.executeStreaming(engine, params);
 * for (auto resultOpt = stream.next(); resultOpt.has_value(); 
 *      resultOpt = stream.next()) {
     *     process(resultOpt.value());
     * }
     * @endcode
     */
    StreamingResult executeStreaming(LuaEngine& engine,
                                     const std::vector<RuleParameter>& parameters);

    // ========================================================================
    // Dependency Analysis
    // ========================================================================
    
    /**
     * @brief Get the topological execution order
     * 
     * Uses Kahn's algorithm for topological sorting:
     * 1. Start with rules that have no dependencies
     * 2. Remove them from the graph
     * 3. Find rules whose dependencies are all satisfied
     * 4. Repeat until all rules are ordered
     * 
     * Within each level, rules are sorted by priority.
     * 
     * @return Rules in dependency-resolved execution order
     */
    [[nodiscard]] std::vector<std::shared_ptr<Rule>> resolveExecutionOrder() const;

    /**
     * @brief Get rules grouped by dependency level
     * 
     * Returns a vector where each inner vector contains rules that can
     * execute in parallel (no dependencies between them).
     * 
     * @return Vector of dependency levels, each level is a vector of rules
     */
    [[nodiscard]] std::vector<std::vector<std::shared_ptr<Rule>>> buildDependencyLevels() const;

    /**
     * @brief Generate a Graphviz DOT representation
     * 
     * Creates a DOT graph showing:
     * - Rule nodes with IDs and expressions
     * - Dependency edges (red, bold)
     * - Parent-child edges (dashed)
     * - Inactive rules (gray)
     * 
     * @return DOT format string for visualization
     * 
     * Example:
     * @code
     * std::ofstream("workflow.dot") << workflow.toGraph();
     * // Then: dot -Tpng workflow.dot > workflow.png
     * @endcode
     */
    [[nodiscard]] std::string toGraph() const;

    // ========================================================================
    // Builder Pattern
    // ========================================================================
    
    /**
     * @brief Builder class for fluent Workflow construction
     * 
     * Provides a fluent API for constructing workflows.
     * 
     * Example:
     * @code
     * auto workflow = Workflow::Builder(1)
     *     .withDescription("Customer validation")
     *     .addRule(rule1)
     *     .addRule(rule2)
     *     .active(true)
     *     .build();
     * @endcode
     */
    class Builder {
    public:
        /**
         * @brief Construct a builder starting with the workflow ID
         * @param workflowId The unique workflow identifier
         */
        explicit Builder(int workflowId);

        /// @brief Set the workflow description
        Builder& withDescription(const std::string& desc);

        /// @brief Add a rule to the workflow
        Builder& addRule(std::shared_ptr<Rule> rule);

        /// @brief Set the active state
        Builder& active(bool active);

        /// @brief Build and return the Workflow
        [[nodiscard]] Workflow build();

    private:
        std::unique_ptr<Workflow> workflow_;  ///< The workflow being built
    };

private:
    // ========================================================================
    // State
    // ========================================================================
    
    bool validated_ = false;               ///< Whether validate() has been called
    bool compiled_ = false;                ///< Whether compile() has been called

    // ========================================================================
    // Engine Pool (for parallel execution)
    // ========================================================================
    
    bool useEnginePool_ = false;           ///< Whether to use the engine pool
    std::unique_ptr<EnginePool> enginePool_; ///< Pool for thread-safe engine access
    std::vector<std::unique_ptr<LuaEngine>> enginePoolStorage_; ///< Storage for pool engines

    /**
     * @brief Acquire an engine from the pool
     * 
     * Returns a pre-compiled engine clone from the pool.
     * Blocks until an engine is available (with timeout).
     * 
     * @return Pointer to engine, or nullptr on timeout
     */
    LuaEngine* acquireEngine();

    /**
     * @brief Return an engine to the pool
     * @param engine The engine to return
     */
    void releaseEngine(LuaEngine* engine);

    // ========================================================================
    // Adaptive Execution State
    // ========================================================================
    
    size_t adaptiveThreshold_ = 4;         ///< Threshold for adaptive execution
    bool autoDetectThreshold_ = false;     ///< Whether to auto-detect threshold
    
    // Performance tracking for auto-detection
    double sequentialAvgTime_ = 0.0;         ///< Rolling average for sequential
    double parallelAvgTime_ = 0.0;           ///< Rolling average for parallel
    size_t sequentialRuns_ = 0;              ///< Number of sequential samples
    size_t parallelRuns_ = 0;                ///< Number of parallel samples

    // ========================================================================
    // Internal Methods
    // ========================================================================
    
    /**
     * @brief Build compilation levels for parallel compilation
     * 
     * Groups rules by their dependencies such that rules in the same
     * level can be compiled concurrently. Uses Kahn's algorithm
     * on the child rule dependency graph.
     * 
     * @return Vector of rule levels, each level is a vector of rules
     */
    [[nodiscard]] std::vector<std::vector<Rule*>> buildCompilationLevels();

    /**
     * @brief Check for circular dependencies
     * 
     * Uses DFS with three-color marking:
     * - White: unvisited
     * - Gray: visiting (in current DFS path) - cycle detected if reached
     * - Black: finished
     * 
     * @throws RuleValidationException if a cycle is found
     */
    void checkCircularDependencies() const;

    /**
     * @brief Collect all action strings from rules
     * 
     * Used for auto-discovery of callback handlers.
     * Recursively searches child rules.
     * 
     * @return Vector of all action strings
     */
    [[nodiscard]] std::vector<std::string> getAllActions() const;

    /**
     * @brief Recursive helper for collecting actions
     * @param ruleList Rules to search
     * @param out Output vector
     */
    void collectActions(const std::vector<std::shared_ptr<Rule>>& ruleList,
                        std::vector<std::string>& out) const;
};

} // namespace fastrules
