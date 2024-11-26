#include "scripting_api.h"
#include "../networking/socket.h"
#include "../util/fs.h"
#include <lauxlib.h>
#include <lua.h>
#include <lua_all.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <synchapi.h>
#include <winsock2.h>

result_t scripting_api_new(scripting_api_t *out) {
    log_header("Initializing Scripting API");
    out->lua_state = luaL_newstate();
    luaL_openlibs(out->lua_state);
    luaL_dostring(out->lua_state, "package.path = package.path .. ';.ds/scripts/?.lua;.ds/scripts/?/init.lua'");

    scripting_api_init_globals(out);
    log_info("Initialized globals.");

    if (!fs_exists("config.lua"))
        fs_save("config.lua", DEFAULT_CONFIG, strlen(DEFAULT_CONFIG));
    luaL_dofile(out->lua_state, "config.lua");
    log_info("Loaded config.");

    if (!fs_direxists("events")) {
        result_t res;
        if ((res = fs_mkdir("events")).is_error) {
            log_error("Failed to create events folder. Are permissions configured correctly?");
            result_discard(res);
            exit(-1);
        }
    }
    if (!fs_direxists("scripts")) {
        result_t res;
        if ((res = fs_mkdir("scripts")).is_error) {
            log_error("Failed to create scripts folder. Are permissions configured correctly?");
            result_discard(res);
            exit(-1);
        }
    }

    fs_recurse("events", (void (*)(const char *, void *))scripting_api_load_file, out);
    log_info("Loaded all events!");

    out->mutex = CreateMutex(NULL, FALSE, "Scripting_API_Mutex");

    return result_no_error();
}

void scripting_api_init_globals(scripting_api_t *self) {
    lua_newtable(self->lua_state);
    lua_setglobal(self->lua_state, "ds");
    lua_getglobal(self->lua_state, "ds");

    lua_newtable(self->lua_state);
    lua_setfield(self->lua_state, -2, "events");

    lua_newtable(self->lua_state);
    lua_setfield(self->lua_state, -2, "config");

    lua_pop(self->lua_state, 1);
}

void scripting_api_cleanup(scripting_api_t *self) {
    lua_close(self->lua_state);
}

void scripting_api_load_file(const char *name, scripting_api_t *self) {
    if (luaL_dofile(self->lua_state, name) != LUA_OK) {
        log_error("Error Loading Event '%s': %s", name, lua_tostring(self->lua_state, -1));
        lua_pop(self->lua_state, 1);
        return;
    }
    log_info("Loaded event '%s'", name);
}

result_t scripting_api_config_number(scripting_api_t *self, const char *name, float *out) {
    lua_getglobal(self->lua_state, "ds");
    lua_getfield(self->lua_state, -1, "config");
    lua_getfield(self->lua_state, -1, name);

    if (!lua_isnumber(self->lua_state, -1)) {
        lua_pop(self->lua_state, 3);
        return result_error("NumberNotFoundErr", "Couldn't find number variable '%s' in the config.", name);
    }

    *out = lua_tonumber(self->lua_state, -1);
    lua_pop(self->lua_state, 3);

    return result_no_error();
}

result_t scripting_api_config_string(scripting_api_t *self, const char *name, char **out) {
    lua_getglobal(self->lua_state, "ds");
    lua_getfield(self->lua_state, -1, "config");
    lua_getfield(self->lua_state, -1, name);

    if (!lua_isstring(self->lua_state, -1)) {
        lua_pop(self->lua_state, 3);
        return result_error("StringNotFoundErr", "Couldn't find string variable '%s' in the config.", name);
    }

    *out = _strdup(lua_tostring(self->lua_state, -1));
    lua_pop(self->lua_state, 3);

    return result_no_error();
}

result_t scripting_api_try_event(scripting_api_t *self, intermediate_t *intermediate) {
    
    return result_no_error();
}