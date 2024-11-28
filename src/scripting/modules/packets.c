#include "packets.h"
#include "../../networking/server.h"
#include "../../networking/client.h"
#include <lauxlib.h>
#include <lua.h>
#include <lua_all.h>
#include <stdint.h>
#include <stdlib.h>

scripting_function_t api_packets_functions[] = {
    { "send_tcp", api_packets_send_tcp },
    { "broadcast_tcp", api_packets_broadcast_tcp },

    { "send_udp", api_packets_send_udp },
    { "broadcast_udp", api_packets_broadcast_udp },

    { "reply", api_packets_reply },
};

__attribute__((constructor)) void api_packets_init(void) {
    scripting_modules[SCRIPTING_MODULES_PACKETS] = (scripting_module_t) {
        .name = "packets",
        .function_count = sizeof(api_packets_functions) / sizeof(scripting_function_t),
        .functions = api_packets_functions,
    };
}

intermediate_t *table_to_intermediate(lua_State *L, char *event, uint32_t reply) {
    intermediate_t *intermediate = intermediate_new(event, reply);
    lua_pushnil(L);

    while (lua_next(L, -2)) {
        // -1 value, -2 key, -3 table
        if (lua_isinteger(L, -2)) {
            log_warn("Lua intermediate error: field [%d] is of unsupported index type integer. It will not be sent.", lua_tointeger(L, -1));
            lua_pop(L, 1);
            continue;
        }

        if (lua_isnumber(L, -1)) {
            intermediate_auto_number_var(intermediate, lua_tostring(L, -2), lua_tonumber(L, -1));
        } else if (lua_isstring(L, -1)) {
            const char *str = lua_tostring(L, -1);
            intermediate_add_var(intermediate, lua_tostring(L, -2), INTERMEDIATE_STRING, (char *)str, strlen(str) + 1);
        } else if (lua_isboolean(L, -1)) {
            uint8_t boolean = lua_toboolean(L, -1);
            intermediate_add_var(intermediate, lua_tostring(L, -2), INTERMEDIATE_U8, &boolean, sizeof(uint8_t));
        } else {
            lua_pop(L, 1);
            continue;
        }

        lua_pop(L, 1);
    }

    intermediate->id = intermediate_generate_id();

    return intermediate;
}

int api_packets_send_tcp(lua_State *L) {
    const char *uuid = luaL_checkstring(L, 1);
    const char *type = luaL_checkstring(L, 2);
    luaL_checktype(L, 3, LUA_TTABLE);

    lua_pushvalue(L, 3);
    intermediate_t * intermediate = table_to_intermediate(L, (char *)type, 0);
    lua_pop(L, 1);

    int len = 0;
    char *buffer = intermediate_to_buffer(intermediate, &len);
    free(intermediate);

    DWORD dwWait = WaitForSingleObject(server.clients_mutex, INFINITE);
    if ((dwWait == 0) || (dwWait == WAIT_ABANDONED)) {
        client_t *c;
        if (!(c = *(client_t **)hashtable_get(&server.clients, (void *)uuid)))
            return 0;

        // Send
        if (send(c->socket, buffer, len, 0) == SOCKET_ERROR)
            client_delete(c);

        ReleaseMutex(server.clients_mutex);
    }

    free(buffer);
    return 0;
}

int api_packets_broadcast_tcp(lua_State *L) {
    const char *type = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);

    lua_pushvalue(L, 2);
    intermediate_t * intermediate = table_to_intermediate(L, (char *)type, 0);
    lua_pop(L, 1);

    int len = 0;
    char *buffer = intermediate_to_buffer(intermediate, &len);
    free(intermediate);

    DWORD dwWait = WaitForSingleObject(server.clients_mutex, INFINITE);
    if ((dwWait == 0) || (dwWait == WAIT_ABANDONED)) {
        uint32_t count = 0;
        pair_t **pairs = hashtable_pairs(&server.clients, &count);
        for (pair_t **pl = pairs; pl < pairs + count; ++pl) {
            client_t *client = *(client_t **)(*pl)->value;
            send(client->socket, buffer, len, 0);
        }

        ReleaseMutex(server.clients_mutex);
    }

    free(buffer);
    return 0;
}

int api_packets_send_udp(lua_State *L) {
    const char *uuid = luaL_checkstring(L, 1);
    const char *type = luaL_checkstring(L, 2);
    luaL_checktype(L, 3, LUA_TTABLE);

    lua_pushvalue(L, 3);
    intermediate_t * intermediate = table_to_intermediate(L, (char *)type, 0);
    lua_pop(L, 1);

    int len = 0;
    char *buffer = intermediate_to_buffer(intermediate, &len);
    free(intermediate);

    DWORD dwWait = WaitForSingleObject(server.clients_mutex, INFINITE);
    if ((dwWait == 0) || (dwWait == WAIT_ABANDONED)) {
        client_t *c;
        if (!(c = *(client_t **)hashtable_get(&server.clients, (void *)uuid)))
            return 0;

        // Send
        if (sendto(server.udp_socket, buffer, len, 0, (struct sockaddr *)&c->address, sizeof(struct sockaddr)) == SOCKET_ERROR)
            client_delete(c);

        ReleaseMutex(server.clients_mutex);
    }

    free(buffer);
    return 0;
}

int api_packets_broadcast_udp(lua_State *L) {
    const char *type = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);

    lua_pushvalue(L, 2);
    intermediate_t * intermediate = table_to_intermediate(L, (char *)type, 0);
    lua_pop(L, 1);

    int len = 0;
    char *buffer = intermediate_to_buffer(intermediate, &len);
    free(intermediate);

    DWORD dwWait = WaitForSingleObject(server.clients_mutex, INFINITE);
    if ((dwWait == 0) || (dwWait == WAIT_ABANDONED)) {
        uint32_t count = 0;
        pair_t **pairs = hashtable_pairs(&server.clients, &count);
        for (pair_t **pl = pairs; pl < pairs + count; ++pl) {
            client_t *client = *(client_t **)(*pl)->value;
            sendto(server.udp_socket, buffer, len, 0, (struct sockaddr *)&client->address, sizeof(struct sockaddr));
        }

        ReleaseMutex(server.clients_mutex);
    }

    free(buffer);
    return 0;
}

int api_packets_reply(lua_State *L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    luaL_checktype(L, 2, LUA_TTABLE);

    uint32_t reply;
    lua_getfield(L, 1, "id");
    if (!lua_isnumber(L, -1))
        return 0;
    reply = lua_tonumber(L, -1);
    lua_pop(L, 1);

    const char *type = NULL;
    lua_getfield(L, 2, "type");
    if (!lua_isstring(L, -1)) {
        lua_pop(L, 1);
        lua_getfield(L, 1, "type");
        if (!lua_isstring(L, -1))
            return 0;
    }
    type = lua_tostring(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 1, "client");
    if (!lua_istable(L, -1))
        return 0;
    lua_getfield(L, -1, "uuid");
    if (!lua_isstring(L, -1))
        return 0;
    const char *uuid = lua_tostring(L, -1);
    lua_pop(L, 2);

    lua_pushvalue(L, 2);

    intermediate_t * intermediate = table_to_intermediate(L, (char *)type, reply);
    lua_pop(L, 1);

    int len = 0;
    char *buffer = intermediate_to_buffer(intermediate, &len);
    free(intermediate);

    DWORD dwWait = WaitForSingleObject(server.clients_mutex, INFINITE);
    if ((dwWait == 0) || (dwWait == WAIT_ABANDONED)) {
        client_t *c;
        if (!(c = *(client_t **)hashtable_get(&server.clients, (void *)uuid)))
            return 0;

        // Send
        if (send(c->socket, buffer, len, 0) == SOCKET_ERROR)
            client_delete(c);

        ReleaseMutex(server.clients_mutex);
    }

    free(buffer);
    return 0;
}