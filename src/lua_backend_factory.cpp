#include "fastrules/lua_backend.hpp"
#include "fastrules/lua_backend_luabridge.hpp"

namespace fastrules {

std::unique_ptr<LuaBackend> LuaBackend::create() {
    return std::make_unique<LuaBridge3Backend>();
}

} // namespace fastrules
