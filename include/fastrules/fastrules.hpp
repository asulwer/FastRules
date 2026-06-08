#pragma once

// Main header that includes everything needed to use fastrules core
// NOTE: JSON loading is in the fastrules-json extension. Include <fastrules/json_loader.hpp> separately.

#include "rule.hpp"
#include "rule_result.hpp"
#include "rule_context.hpp"
#include "workflow.hpp"
#include "lua_engine.hpp"
#include "type_registry.hpp"
#include "enum_registry.hpp"
#include "logger.hpp"
#include "streaming_result.hpp"
#include "async_registry.hpp"
#include "execution_tracer.hpp"
#include "parameter_validator.hpp"
#include "expression_validator.hpp"
#include "rate_limiter.hpp"
#include "performance_counters.hpp"
#include "aot_compiler.hpp"
#include "rule_versioning.hpp"

// Version info
#define FASTRULES_VERSION_MAJOR 0
#define FASTRULES_VERSION_MINOR 1
#define FASTRULES_VERSION_PATCH 0
