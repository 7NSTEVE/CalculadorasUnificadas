#pragma once

#include <cstddef>
#include <cstdint>

struct casioemux_lua_State
{
    alignas(std::max_align_t) unsigned char scratch[512]{};
    std::size_t used = 0;
};

using lua_State = casioemux_lua_State;

namespace casioemux_lua_stub
{
    inline thread_local lua_State *bound_state = nullptr;

    class ScopedBinding
    {
    public:
        explicit ScopedBinding(lua_State *state) noexcept
            : previous_(bound_state)
        {
            if (state)
                state->used = 0;
            bound_state = state;
        }

        ~ScopedBinding()
        {
            bound_state = previous_;
        }

        ScopedBinding(const ScopedBinding &) = delete;
        ScopedBinding &operator=(const ScopedBinding &) = delete;

    private:
        lua_State *previous_;
    };
}

typedef int (*lua_CFunction)(lua_State *);
typedef long long lua_Integer;
typedef double lua_Number;

#define LUA_OK 0
#define LUA_REFNIL (-1)
#define LUA_NOREF (-2)
#define LUA_REGISTRYINDEX (-1001000)
#define LUA_TTABLE 5
#define LUA_TSTRING 4
#define LUA_TNUMBER 3

inline void *lua_newuserdata(lua_State *, size_t size)
{
    static thread_local lua_State fallback;
    lua_State *state = casioemux_lua_stub::bound_state
        ? casioemux_lua_stub::bound_state
        : &fallback;

    constexpr std::size_t alignment = alignof(std::max_align_t);
    const std::size_t aligned = (state->used + alignment - 1) & ~(alignment - 1);
    if (size > sizeof(state->scratch) || aligned > sizeof(state->scratch) - size)
        return nullptr;

    void *memory = state->scratch + aligned;
    state->used = aligned + size;
    return memory;
}

inline void lua_newtable(lua_State *) {}
inline void lua_pushcfunction(lua_State *, lua_CFunction) {}
inline void lua_pushinteger(lua_State *, lua_Integer) {}
inline void lua_pushnumber(lua_State *, lua_Number) {}
inline void lua_pushstring(lua_State *, const char *) {}
inline void lua_setfield(lua_State *, int, const char *) {}
inline void lua_setmetatable(lua_State *, int) {}
inline void lua_setglobal(lua_State *, const char *) {}
inline void lua_pop(lua_State *, int) {}
inline int lua_gettop(lua_State *) { return 0; }
inline const void *lua_topointer(lua_State *, int) { return nullptr; }
inline const char *lua_tostring(lua_State *, int) { return ""; }
inline lua_Integer lua_tointeger(lua_State *, int) { return 0; }
inline lua_Number lua_tonumber(lua_State *, int) { return 0.0; }
inline lua_Integer lua_tointegerx(lua_State *, int, int *isnum)
{
    if (isnum) *isnum = 0;
    return 0;
}
inline int lua_geti(lua_State *, int, lua_Integer) { return 0; }
inline int lua_getfield(lua_State *, int, const char *) { return 0; }
inline int lua_pcall(lua_State *, int, int, int) { return LUA_OK; }
inline int luaL_error(lua_State *, const char *, ...) { return 0; }
inline int luaL_ref(lua_State *, int) { return LUA_REFNIL; }
inline void luaL_unref(lua_State *, int, int) {}
