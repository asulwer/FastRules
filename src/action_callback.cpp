/**
 * @file action_callback.cpp
 * @brief Action callback stub implementation
 * 
 * This file exists for build system compatibility.
 * All ActionCallbacks methods are defined inline in the header
 * (action_callback.hpp) for performance reasons.
 * 
 * Why a separate .cpp file?
 * - Some build systems require a .cpp for every header
 * - CMake target sources may expect this file
 * - Linker compatibility (especially for shared libraries)
 * - Future-proof: if methods move out-of-line, they're here
 * 
 * The ActionCallbacks class provides:
 * - Type-safe action registration
 * - Lambda storage with std::function
 * - Execution with exception translation
 * - Parameter binding from RuleContext
 * 
 * @see action_callback.hpp for full implementation
 */

#include "fastrules/action_callback.hpp"

namespace fastrules {

// All ActionCallbacks methods are inline in the header.
// This file exists for build system compatibility.

} // namespace fastrules
