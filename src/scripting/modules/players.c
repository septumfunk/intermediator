#include "players.h"
#include "../../structures/hashtable.h"
#include "../../networking/server.h"
#include "../../networking/client.h"
#include <lua.h>

scripting_function_t api_players_functions[] = {
    { "kick", api_players_kick },
};

__attribute__((constructor)) void api_players_init(void) {
    scripting_modules[SCRIPTING_MODULES_PLAYERS] = (scripting_module_t) {
        .name = "players",
        .function_count = sizeof(api_players_functions) / sizeof(scripting_function_t),
        .functions = api_players_functions,
    };
}

int api_players_kick(lua_State *L) {
    const char *uuid = luaL_checkstring(L, 1);
    const char *reason = luaL_checkstring(L, 2);

    void *ptr = hashtable_get(&server.clients, (void *)uuid);
    client_t *c;
    if (!(ptr = hashtable_get(&server.clients, (void *)uuid)) || !(c = *(client_t **)ptr))
        return 0;

    client_kick(c, reason);

    return 0;
}