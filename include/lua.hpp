// lua.hpp
// C++ wrapper for Lua C headers.
// LuaBridge3 expects <lua.hpp> but PUC-Rio Lua doesn't ship one.
// This file is part of FastRules -- placed in the include path
// so that #include <lua.hpp> works regardless of which Lua
// backend (PUC-Rio, LuaJIT) is being used.
#pragma once

extern "C" {
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}
