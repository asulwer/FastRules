#include "fastrules/lua_backend.hpp"
#include "fastrules/lua_backend_sol2.hpp"

#ifdef FASTRULES_USE_LUABRIDGE3
#include "fastrules/lua_backend_luabridge.hpp"
#endif

namespace fastrules {

std::unique_ptr<LuaBackend> LuaBackend::create() {
#ifdef FASTRULES_USE_LUABRIDGE3
    return std::make_unique<LuaBridge3Backend>();
#else
    return std::make_unique<Sol2Backend>();
#endif
}

} // namespace fastrules
