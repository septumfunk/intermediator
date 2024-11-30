#include "scripting_api.h"
#include "modules/modules.h"
#include "../networking/socket.h"
#include "../win32/fs.h"
#include "intermediate.h"
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

    scripting_api_init_globals(out);
    log_info("Initialized globals.");

    if (!fs_exists("config.lua"))
        fs_save("config.lua", DEFAULT_CONFIG, strlen(DEFAULT_CONFIG));
    luaL_dofile(out->lua_state, "config.lua");
    log_info("Loaded config.");

    if (!fs_direxists("scripts")) {
        result_t res;
        if ((res = fs_mkdir("scripts")).is_error) {
            log_error("Failed to create scripts folder. Are permissions configured correctly?");
            result_discard(res);
            exit(-1);
        }
    }

    fs_recurse("scripts", (void (*)(const char *, void *))scripting_api_load_file, out);
    log_info("Loaded all scripts!");

    log_header("Initializing Scripting Modules");
    lua_getglobal(out->lua_state, "ds");
    for (scripting_module_t *module = scripting_modules; module < scripting_modules + SCRIPTING_MODULES_COUNT; ++module) {
        if (!module->name || !module->functions)
            continue;
        log_info("Registering module '%s'", module->name);
        lua_newtable(out->lua_state);
        lua_setfield(out->lua_state, -2, module->name);
        lua_getfield(out->lua_state, -1, module->name);

        for (scripting_function_t *func = module->functions; func < module->functions + module->function_count; ++func) {
            log_info("Registering function '%s'", func->name);
            lua_pushcfunction(out->lua_state, func->function);
            lua_setfield(out->lua_state, -2, func->name);
        }

        lua_pop(out->lua_state, 1);
    }

    out->mutex = mutex_new();

    return result_no_error();
}

void scripting_api_init_globals(scripting_api_t *self) {
    mutex_lock(self->mutex);

    lua_newtable(self->lua_state);
    lua_setglobal(self->lua_state, "ds");
    lua_getglobal(self->lua_state, "ds");

    lua_newtable(self->lua_state);
    lua_setfield(self->lua_state, -2, "events");

    lua_newtable(self->lua_state);
    lua_setfield(self->lua_state, -2, "config");

    lua_newtable(self->lua_state);
    lua_setfield(self->lua_state, -2, "clients");

    lua_pop(self->lua_state, 1);

    mutex_release(self->mutex);
}

void scripting_api_cleanup(scripting_api_t *self) {
    mutex_lock(self->mutex);
    lua_close(self->lua_state);
    mutex_release(self->mutex);

    mutex_delete(self);
}

void scripting_api_load_file(const char *name, scripting_api_t *self) {
    mutex_lock(self->mutex);
    if (luaL_dofile(self->lua_state, name) != LUA_OK) {
        log_error("Error Loading Event '%s': %s", name, lua_tostring(self->lua_state, -1));
        lua_pop(self->lua_state, 1);
        mutex_release(self->mutex);
        return;
    }
    log_info("Loaded event '%s'", name);
    mutex_release(self->mutex);
}

result_t scripting_api_config_number(scripting_api_t *self, const char *name, float *out, float def) {
    mutex_lock(self->mutex);

    lua_getglobal(self->lua_state, "ds");
    lua_getfield(self->lua_state, -1, "config");
    lua_getfield(self->lua_state, -1, name);

    if (!lua_isnumber(self->lua_state, -1)) {
        lua_pop(self->lua_state, 1);
        lua_pushnumber(self->lua_state, def);
        lua_setfield(self->lua_state, -1, name);
        lua_getfield(self->lua_state, -1, name);
    }

    *out = lua_tonumber(self->lua_state, -1);
    lua_pop(self->lua_state, 3);

    mutex_release(self->mutex);

    return result_no_error();
}

result_t scripting_api_config_string(scripting_api_t *self, const char *name, char **out, char *def) {
    mutex_lock(self->mutex);

    lua_getglobal(self->lua_state, "ds");
    lua_getfield(self->lua_state, -1, "config");
    lua_getfield(self->lua_state, -1, name);

    if (!lua_isstring(self->lua_state, -1)) {
        lua_pop(self->lua_state, 1);
        lua_pushstring(self->lua_state, def);
        lua_setfield(self->lua_state, -1, name);
        lua_getfield(self->lua_state, -1, name);
    }

    *out = _strdup(lua_tostring(self->lua_state, -1));
    lua_pop(self->lua_state, 3);

    mutex_release(self->mutex);

    return result_no_error();
}

void scripting_api_create_client(scripting_api_t *self, char *uuid, struct sockaddr_in addr) {
    mutex_lock(self->mutex);

    lua_getglobal(self->lua_state, "ds");
    lua_getfield(self->lua_state, -1, "clients");
    lua_getfield(self->lua_state, -1, uuid);
    if (lua_istable(self->lua_state, -1)) {
        lua_settop(self->lua_state, 0);
        mutex_release(self->mutex);
        return;
    }
    lua_pop(self->lua_state, 1);

    lua_newtable(self->lua_state);

    lua_pushstring(self->lua_state, uuid);
    lua_setfield(self->lua_state, -2, "uuid");

    lua_pushstring(self->lua_state, inet_ntoa(addr.sin_addr));
    lua_setfield(self->lua_state, -2, "ip");

    lua_pushnumber(self->lua_state, ntohs(addr.sin_port));
    lua_setfield(self->lua_state, -2, "port");

    lua_setfield(self->lua_state, -2, uuid);
    lua_getfield(self->lua_state, -1, uuid);

    lua_settop(self->lua_state, 0);

    mutex_release(self->mutex);
}

void scripting_api_delete_client(scripting_api_t *self, char *uuid) {
    mutex_lock(self->mutex);

    lua_getglobal(self->lua_state, "ds");
    lua_getfield(self->lua_state, -1, "clients");
    lua_getfield(self->lua_state, -1, uuid);
    if (!lua_istable(self->lua_state, -1)) {
        log_error("No client exists with uuid '%s'", uuid);
        lua_settop(self->lua_state, 0);
        mutex_release(self->mutex);
        return;
    }

    lua_pushnil(self->lua_state);
    lua_setfield(self->lua_state, -2, uuid);

    mutex_release(self->mutex);
}

result_t scripting_api_try_event(scripting_api_t *self, intermediate_t *intermediate) {
    mutex_lock(self->mutex);

    lua_getglobal(self->lua_state, "ds");
    lua_getfield(self->lua_state, -1, "events");
    lua_getfield(self->lua_state, -1, intermediate->type);
    if (!lua_isfunction(self->lua_state, -1)) {
        mutex_release(self->mutex);
        lua_settop(self->lua_state, 0);
        return result_error("EventNotFoundErr", "Unable to locate event '%s'", intermediate->type);
    }

    lua_newtable(self->lua_state);

    lua_getglobal(self->lua_state, "ds");
    lua_getfield(self->lua_state, -1, "clients");
    lua_getfield(self->lua_state, -1, intermediate->client_uuid);
    if (!lua_istable(self->lua_state, -1)) {
        mutex_release(self->mutex);
        lua_settop(self->lua_state, 0);
        return result_error("EventNotFoundErr", "Unable to locate client '%s'", intermediate->client_uuid);
    }
    lua_remove(self->lua_state, -2);
    lua_remove(self->lua_state, -2);
    lua_setfield(self->lua_state, -2, "client");

    lua_pushnumber(self->lua_state, (double)intermediate->id);
    lua_setfield(self->lua_state, -2, "id");
    lua_pushnumber(self->lua_state, intermediate->reply);
    lua_setfield(self->lua_state, -2, "reply");
    lua_pushstring(self->lua_state, intermediate->type);
    lua_setfield(self->lua_state, -2, "type");

    intermediate_variable_t *head = intermediate->variables;
    while (head) {
        switch (head->type) {
            case INTERMEDIATE_STRING:
                lua_pushstring(self->lua_state, head->value);
                lua_setfield(self->lua_state, -2, head->name);
                break;

            case INTERMEDIATE_S8:
                lua_pushnumber(self->lua_state, *(int8_t *)head->value);
                lua_setfield(self->lua_state, -2, head->name);
                break;
            case INTERMEDIATE_S16:
                lua_pushnumber(self->lua_state, *(int16_t *)head->value);
                lua_setfield(self->lua_state, -2, head->name);
                break;
            case INTERMEDIATE_S32:
                lua_pushnumber(self->lua_state, *(int32_t *)head->value);
                lua_setfield(self->lua_state, -2, head->name);
                break;
            case INTERMEDIATE_S64:
                lua_pushnumber(self->lua_state, *(int64_t *)head->value);
                lua_setfield(self->lua_state, -2, head->name);
                break;

            case INTERMEDIATE_U8:
                lua_pushnumber(self->lua_state, *(uint8_t *)head->value);
                lua_setfield(self->lua_state, -2, head->name);
                break;
            case INTERMEDIATE_U16:
                lua_pushnumber(self->lua_state, *(uint16_t *)head->value);
                lua_setfield(self->lua_state, -2, head->name);
                break;
            case INTERMEDIATE_U32:
                lua_pushnumber(self->lua_state, *(uint32_t *)head->value);
                lua_setfield(self->lua_state, -2, head->name);
                break;
            case INTERMEDIATE_U64:
                lua_pushnumber(self->lua_state, *(uint64_t *)head->value);
                lua_setfield(self->lua_state, -2, head->name);
                break;

            case INTERMEDIATE_F32:
                lua_pushnumber(self->lua_state, *(float *)head->value);
                lua_setfield(self->lua_state, -2, head->name);
                break;
            case INTERMEDIATE_F64:
                lua_pushnumber(self->lua_state, *(double *)head->value);
                lua_setfield(self->lua_state, -2, head->name);
                break;
        }
        head = head->next;
    }

    if (lua_pcall(self->lua_state, 1, 0, 0) != LUA_OK) {
        mutex_release(self->mutex);
        result_t res = result_error("LuaEventErr", lua_tostring(self->lua_state, -1));
        lua_settop(self->lua_state, 0);
        return res;
    }

    lua_settop(self->lua_state, 0);

    mutex_release(self->mutex);
    return result_no_error();
}