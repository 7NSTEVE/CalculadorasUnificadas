/*
 * Sustituto vacio de Lua para el nucleo del emulador.
 *
 * CPU.cpp y MMU.cpp usan Lua unicamente para exponer ganchos de depuracion a la consola
 * del emulador de escritorio: publicar los objetos `cpu`, `code` y `data`, y permitir
 * watchpoints de lectura/escritura. Nada de eso interviene en el calculo.
 *
 * En vez de editar esos dos ficheros (y arriesgarme a romper sin querer la logica de
 * emulacion, que ya paso 36 pruebas), se anulan las llamadas aqui. Los watchpoints
 * quedan permanentemente a LUA_REFNIL, asi que las ramas que los usan nunca se ejecutan.
 */
#pragma once

#include <cstddef>
#include <cstdint>

/*
 * El port no enlaza Lua, pero CPU/MMU conservan tres llamadas a
 * lua_newuserdata() durante la construccion. El buffer que devuelve este stub
 * tiene que respetar dos propiedades aunque su contenido nunca se consulte:
 *
 *   - alineacion suficiente para escribir un CPU * o MMU *;
 *   - aislamiento entre instancias que se construyen concurrentemente.
 *
 * Un unico `static unsigned char[64]` no cumplia ninguna: podia estar
 * desalineado y dos creaciones en hilos distintos escribian sobre los mismos
 * bytes. Cada CalcEmu enlaza ahora su propio estado efimero mientras se
 * construye. El enlace es thread_local para que dos construcciones simultaneas
 * no se interfieran.
 */
struct lua_State
{
    alignas(std::max_align_t) unsigned char scratch[256]{};
    std::size_t used = 0;
};

namespace calcemu_lua_stub
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

#define LUA_OK 0
#define LUA_REFNIL (-1)
#define LUA_NOREF (-2)
#define LUA_REGISTRYINDEX (-1001000)
#define LUA_TTABLE 5
#define LUA_TSTRING 4
#define LUA_TNUMBER 3

/**
 * Devuelve memoria valida y escribible: el codigo original hace
 * `*(MMU **)lua_newuserdata(...) = this;` y un puntero nulo reventaria.
 * El contenido no lo lee nadie, porque no hay interprete detras.
 */
inline void *lua_newuserdata(lua_State *, size_t size)
{
    // Fuera de calcemu_create() se mantiene un respaldo aislado por hilo. No
    // deberia usarse en este port, pero evita reintroducir memoria global
    // compartida si el nucleo agrega otra llamada de depuracion en el futuro.
    static thread_local lua_State fallback;
    lua_State *state = calcemu_lua_stub::bound_state
        ? calcemu_lua_stub::bound_state
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
inline void lua_pushstring(lua_State *, const char *) {}
inline void lua_setfield(lua_State *, int, const char *) {}
inline void lua_setmetatable(lua_State *, int) {}
inline void lua_setglobal(lua_State *, const char *) {}
inline void lua_pop(lua_State *, int) {}
inline int lua_gettop(lua_State *) { return 0; }
inline const void *lua_topointer(lua_State *, int) { return nullptr; }
inline const char *lua_tostring(lua_State *, int) { return ""; }
inline lua_Integer lua_tointeger(lua_State *, int) { return 0; }
inline lua_Integer lua_tointegerx(lua_State *, int, int *isnum)
{
    if (isnum) *isnum = 0;
    return 0;
}
inline int lua_geti(lua_State *, int, lua_Integer) { return 0; }
inline int lua_pcall(lua_State *, int, int, int) { return LUA_OK; }
inline int luaL_error(lua_State *, const char *, ...) { return 0; }
inline int luaL_ref(lua_State *, int) { return LUA_REFNIL; }
inline void luaL_unref(lua_State *, int, int) {}
