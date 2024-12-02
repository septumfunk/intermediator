#include "tables.h"

scripting_function_t api_tables_functions[] = {
    { "merge", api_tables_merge },
    { "copy", api_tables_copy },
};

__attribute__((constructor)) void api_tables_init(void) {
    scripting_modules[SCRIPTING_MODULES_TABLES] = (scripting_module_t) {
        .name = "tables",
        .function_count = sizeof(api_tables_functions) / sizeof(scripting_function_t),
        .functions = api_tables_functions,
    };
}

int api_tables_merge(lua_State *L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    luaL_checktype(L, 2, LUA_TTABLE);

    lua_newtable(L);

    lua_pushnil(L);
    while (lua_next(L, 1)) {
        lua_pushvalue(L, -2);
        lua_insert(L, -2);
        lua_settable(L, -4);
    }

    lua_pushnil(L);
    while (lua_next(L, 2)) {
        lua_pushvalue(L, -2);
        lua_insert(L, -2);
        lua_settable(L, -4);
    }

    return 1;
}

int api_tables_copy(lua_State *L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    luaL_checktype(L, 2, LUA_TTABLE);

    lua_pushnil(L);
    while (lua_next(L, 1)) {
        lua_pushvalue(L, -2);
        lua_insert(L, -2);
        lua_settable(L, 2);
    }

    return 0;
}