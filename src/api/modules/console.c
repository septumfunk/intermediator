#include "console.h"
#include "../../io/console.h"
#include <lauxlib.h>

scripting_function_t api_console_functions[] = {
    { "log", api_console_log},
    { "warn", api_console_warn },
    { "error", api_console_error },
    { "header", api_console_header },
};

__attribute__((constructor)) void api_console_init(void) {
    scripting_modules[SCRIPTING_MODULES_CONSOLE] = (scripting_module_t) {
        .name = "console",
        .function_count = sizeof(api_console_functions) / sizeof(scripting_function_t),
        .functions = api_console_functions,
    };
}

int api_console_log(lua_State *L) {
    console_log(luaL_checkstring(L, 1));
    return 0;
}

int api_console_warn(lua_State *L) {
    console_warn(luaL_checkstring(L, 1));
    return 0;
}

int api_console_error(lua_State *L) {
    console_error(luaL_checkstring(L, 1));
    return 0;
}

int api_console_header(lua_State *L) {
    console_header(luaL_checkstring(L, 1));
    return 0;
}
