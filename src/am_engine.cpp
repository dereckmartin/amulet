#include "amulet.h"

am_program_param_name_info *am_param_name_map = NULL;

static void init_reserved_refs(lua_State *L);
static void open_stdlualibs(lua_State *L);
static bool run_embedded_scripts(lua_State *L, bool worker);

lua_State *am_init_engine(bool worker) {
    lua_State *L = luaL_newstate();
    init_reserved_refs(L);
    open_stdlualibs(L);
    am_open_math_module(L);
    am_open_buffer_module(L);
    if (!worker) {
        am_init_param_name_map(L);
        am_init_actions();
        am_open_window_module(L);
        am_open_node_module(L);
        am_open_program_module(L);
    }
    if (!run_embedded_scripts(L, worker)) {
        lua_close(L);
        return NULL;
    }
    am_set_globals_metatable(L);
    return L;
}

void am_destroy_engine(lua_State *L) {
    lua_close(L);
}

static void init_reserved_refs(lua_State *L) {
    int i, j;
    do {
        lua_pushboolean(L, 1);
        i = luaL_ref(L, LUA_REGISTRYINDEX);
    } while (i < AM_RESERVED_REFS_START - 1);
    if (i != AM_RESERVED_REFS_START - 1) {
        am_abort("Internal Error: AM_RESERVED_REFS_START too low\n");
    }
    for (i = AM_RESERVED_REFS_START; i < AM_RESERVED_REFS_END; i++) {
        lua_pushboolean(L, 1);
        j = luaL_ref(L, LUA_REGISTRYINDEX);
        if (i != j) {
            am_abort("Internal Error: non-contiguous refs\n");
        }
        j++;
    }
}

static void open_stdlualibs(lua_State *L) {
    am_requiref(L, "base",      luaopen_base);
    am_requiref(L, "package",   luaopen_package);
    am_requiref(L, "math",      luaopen_math);
#ifndef AM_LUAJIT
    // luajit opens coroutine in luaopen_base
    am_requiref(L, "coroutine", luaopen_coroutine);
#endif
    am_requiref(L, "string",    luaopen_string);
    am_requiref(L, "table",     luaopen_table);
    am_requiref(L, "os",        luaopen_os);
    am_requiref(L, "debug",     luaopen_debug);

#ifdef AM_LUAJIT
    am_requiref(L, "ffi",       luaopen_ffi);
    am_requiref(L, "jit",       luaopen_jit);
#endif
}

#define MAX_CHUNKNAME_SIZE 100

static bool run_embedded_scripts(lua_State *L, bool worker) {
    am_embedded_lua_script *script = &am_embedded_lua_scripts[0];
    char chunkname[MAX_CHUNKNAME_SIZE];
    while (script->filename != NULL) {
        snprintf(chunkname, MAX_CHUNKNAME_SIZE, "@%s", script->filename);
        chunkname[MAX_CHUNKNAME_SIZE-1] = 0;
        int r = luaL_loadbuffer(L, script->script, strlen(script->script), chunkname);
        if (r == LUA_OK) {
            if (!am_call(L, 0, 0)) {
                return false;
            }
        } else {
            const char *msg = lua_tostring(L, -1);
            am_report_error(msg);
            return false;
        }
        script++;
    }
    return true;
}
