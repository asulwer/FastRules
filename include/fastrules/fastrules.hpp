/**
 * @file fastrules.hpp
 * @brief Main FastRules header - convenience include for the entire library
 * 
 * This is the primary entry point for users of the FastRules library.
 * Including this file provides access to all core functionality:
 * - Rule definition and execution
 * - Workflow management
 * - Lua scripting engine
 * - Type registration
 * - Action callbacks
 * - Async/concurrent execution
 * - Performance monitoring
 * - Security features
 * 
 * The library is organized into several namespaces and modules:
 * - Core: Rule, Workflow, RuleContext, RuleResult
 * - Engine: LuaEngine, LuaBackend, LuaBridge3Backend
 * - Types: TypeRegistry for binding C++ types to Lua
 * - Actions: ActionCallbacks for registering C++ functions as Lua actions
 * - Extensions (separate): JSON, XML, DB persistence
 * 
 * Example usage:
 * @code
 * #include <fastrules.hpp>
 * 
 * fastrules::LuaEngine engine;
 * 
 * auto rule = fastrules::Rule::Builder(1)
 *     .withExpression("age >= 18")
 *     .withAction("status = 'adult'")
 *     .build();
 * 
 * rule.compile(engine);
 * 
 * fastrules::RuleContext context;
 * auto result = rule.execute(engine, context, 
 *     {{"age", 25}});
 * @endcode
 * 
 * Thread Safety:
 * - LuaEngine: Thread-safe for concurrent rule execution using clones
 * - Rule: Not thread-safe; compile once, then each thread uses its own engine
 * - Workflow: Thread-safe for execution after compilation
 * - RuleContext: Thread-safe (uses shared_mutex internally)
 * 
 * Performance Considerations:
 * - Compile rules once, execute many times
 * - Use engine.clone() for parallel execution
 * - Enable caching for frequently-evaluated rules
 * - Use executeParallel() for independent rules
 */

#pragma once

// ============================================================================
// Core Components
// ============================================================================

/// @brief Rule definition - condition/action pairs
#include "fastrules/rule.hpp"

/// @brief Rule evaluation context - stores intermediate results and variables
#include "fastrules/rule_context.hpp"

/// @brief Rule execution result - success/failure with metadata
#include "fastrules/rule_result.hpp"

/// @brief Workflow orchestration - ordered rule execution with dependencies
#include "fastrules/workflow.hpp"

// ============================================================================
// Lua Engine & Scripting
// ============================================================================

/// @brief Main Lua scripting engine - compiles and executes Lua expressions
#include "fastrules/lua_engine.hpp"

/// @brief Abstract Lua backend interface
#include "fastrules/lua_backend.hpp"

/// @brief LuaBridge3 backend implementation
#include "fastrules/lua_backend_luabridge.hpp"

// ============================================================================
// Type System & Callbacks
// ============================================================================

/// @brief Type registration for binding C++ types to Lua
#include "fastrules/type_registry.hpp"

/// @brief Action callback registration
#include "fastrules/action_callback.hpp"

/// @brief Convenience macros for type registration
#include "fastrules/type_registration_macro.hpp"

// ============================================================================
// Async & Parallel Execution
// ============================================================================

/// @brief Async workflow execution with thread pool
#include "fastrules/async_workflow.hpp"

/// @brief Engine pool for managing cloned Lua states
#include "fastrules/engine_pool.hpp"

// ============================================================================
// Monitoring & Observability
// ============================================================================

/// @brief Execution tracing for debugging workflows
#include "fastrules/execution_tracer.hpp"

/// @brief Global performance counters
#include "fastrules/performance_counters.hpp"

// ============================================================================
// Security & Safety
// ============================================================================

/// @brief Expression validation for dangerous patterns
#include "fastrules/expression_validator.hpp"

/// @brief Parameter type validation
#include "fastrules/parameter_validator.hpp"

/// @brief Token bucket rate limiter
#include "fastrules/rate_limiter.hpp"

// ============================================================================
// Persistence & Versioning
// ============================================================================

/// @brief Rule versioning and rollback
#include "fastrules/rule_versioning.hpp"

/// @brief Ahead-of-time bytecode compilation
#include "fastrules/aot_compiler.hpp"

/// @brief Streaming result generator
#include "fastrules/streaming_result.hpp"

// ============================================================================
// Utilities
// ============================================================================

/// @brief Logging utilities
#include "fastrules/logger.hpp"

/// @brief DLL export macros
#include "fastrules/fastrules_export.hpp"
