#include "scripting_api.h"
#include "../networking/socket.h"
#include "../util/fs.h"
#include <lauxlib.h>
#include <lua.h>
#include <lua_all.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>

scripting_api_t scripting_api;

void scripting_api_init(void) {
    log_header("Initializing Scripting API");
    scripting_api.lua_state = luaL_newstate();
    luaL_openlibs(scripting_api.lua_state);
    luaL_dostring(scripting_api.lua_state, "package.path = package.path .. ';.ds/scripts/?.lua;.ds/scripts/?/init.lua'");

    if (!fs_exists("config.lua"))
        fs_save("config.lua", DEFAULT_CONFIG, strlen(DEFAULT_CONFIG));
    luaL_dofile(scripting_api.lua_state, "config.lua");
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

    scripting_api_init_globals();
    log_info("Initialized globals.");

    fs_recurse("events", scripting_api_load_file);
    log_info("Loaded all events!");
}

void scripting_api_init_globals(void) {
    lua_newtable(scripting_api.lua_state);
    lua_setglobal(scripting_api.lua_state, "ds");
    lua_getglobal(scripting_api.lua_state, "ds");

    lua_newtable(scripting_api.lua_state);
    lua_setfield(scripting_api.lua_state, -2, "events");

    lua_pop(scripting_api.lua_state, 1);
}

void scripting_api_cleanup(void) {
    lua_close(scripting_api.lua_state);
}

void scripting_api_load_file(const char *name) {
    if (luaL_dofile(scripting_api.lua_state, name) != LUA_OK) {
        log_error("Error Loading Event '%s': %s", name, lua_tostring(scripting_api.lua_state, -1));
        lua_pop(scripting_api.lua_state, 1);
        return;
    }
    log_info("Loaded event '%s'", name);
}

result_t scripting_api_config_number(const char *name, float *out) {
    lua_getglobal(scripting_api.lua_state, "config");
    lua_getfield(scripting_api.lua_state, -1, name);

    if (!lua_isnumber(scripting_api.lua_state, -1)) {
        lua_pop(scripting_api.lua_state, 2);
        return result_error("NumberNotFoundErr", "Couldn't find number variable '%s' in the config.", name);
    }

    *out = lua_tonumber(scripting_api.lua_state, -1);
    lua_pop(scripting_api.lua_state, 2);
}

result_t scripting_api_config_string(const char *name, char **out) {
    lua_getglobal(scripting_api.lua_state, "config");
    lua_getfield(scripting_api.lua_state, -1, name);

    if (!lua_isstring(scripting_api.lua_state, -1)) {
        lua_pop(scripting_api.lua_state, 2);
        return result_error("StringNotFoundErr", "Couldn't find string variable '%s' in the config.", name);
    }

    *out = _strdup(lua_tostring(scripting_api.lua_state, -1));
    lua_pop(scripting_api.lua_state, 2);
}

result_t scripting_api_try_event(const char *event_name, SOCKET sock) {
    intermediate_control_e last_control = INTERMEDIATE_NONE;
    bool processing = true;
    while (processing) {
        char control;
        char *event_type = NULL;
        if (recv(sock, &control, sizeof(char), 0) == SOCKET_ERROR)
            return result_error("SocketReadErr", "Failed to read intermediate data from the socket.");
        switch (control) {
            case INTERMEDIATE_HEADER:
                if (last_control != INTERMEDIATE_NONE) {
                    lua_settop(scripting_api.lua_state, 0);
                    return result_error("InvalidIntermediateErr", "The Intermediate's data was not laid out correctly.");
                }

                intermediate_header_t header;
                if (recv(sock, (char *)&header, sizeof(intermediate_header_t), 0) == SOCKET_ERROR)
                    return result_error("SocketReadErr", "Failed to read intermediate data from the socket.");

                switch (header.version) {
                    case INTERMEDIATE_VERSION_1_0:
                        lua_newtable(scripting_api.lua_state);

                        lua_pushnumber(scripting_api.lua_state, header.version);
                        lua_setfield(scripting_api.lua_state, -2, "version");

                        lua_pushnumber(scripting_api.lua_state, header.id);
                        lua_setfield(scripting_api.lua_state, -2, "id");

                        lua_pushnumber(scripting_api.lua_state, header.reply);
                        lua_setfield(scripting_api.lua_state, -2, "reply");

                        char *type = NULL;
                        result_t res;
                        if ((res = socket_read_string(sock, &type, 32)).is_error) {
                            lua_settop(scripting_api.lua_state, 0);
                            return result_error("InvalidIntermediateErr", "The Intermediate's type was unreadable.");
                        }
                        lua_pushstring(scripting_api.lua_state, type);
                        lua_setfield(scripting_api.lua_state, -2, "type");
                        event_type = type;
                        break;
                    default:
                        return result_error("IntermediateVersionMismatchErr", "Intermediate version %f isn't supported by this server.", header.version);
                }

                last_control = INTERMEDIATE_HEADER;
                break;

            case INTERMEDIATE_VARIABLE:
                if (last_control != INTERMEDIATE_HEADER && last_control != INTERMEDIATE_VARIABLE) {
                    lua_settop(scripting_api.lua_state, 0);
                    return result_error("InvalidIntermediateErr", "The Intermediate's data was laid out incorrectly.");
                }

                char *name = NULL;
                result_t res;
                if ((res = socket_read_string(sock, &name, 32)).is_error) {
                    free(name);
                    result_discard(res);
                    lua_settop(scripting_api.lua_state, 0);
                    return result_error("InvalidIntermediateErr", "The Intermediate's variable name was unreadable.");
                }

                char type;
                if (recv(sock, &type, sizeof(char), 0) == SOCKET_ERROR) {
                    free(name);
                    lua_settop(scripting_api.lua_state, 0);
                    return result_error("InvalidIntermediateErr", "The Intermediate's variable type was unreadable.");
                }

                switch (type) {
                    case INTERMEDIATE_STRING:
                        char *string = NULL;
                        result_t res;
                        if ((res = socket_read_string(sock, &string, 128)).is_error) {
                            free(string);
                            free(name);
                            lua_settop(scripting_api.lua_state, 0);
                            return result_error("InvalidIntermediateErr", res.description);
                            result_discard(res);
                        }

                        lua_pushstring(scripting_api.lua_state, string);
                        break;

                    case INTERMEDIATE_S8: {
                        int8_t value;
                        if (recv(sock, (char *)&value, sizeof(value), 0) == SOCKET_ERROR) {
                            free(name);
                            lua_settop(scripting_api.lua_state, 0);
                            return result_error("InvalidIntermediateErr", "The Intermediate's variable value is unreadable.");
                        }
                        lua_pushnumber(scripting_api.lua_state, (double)value);
                        break;
                    }
                    case INTERMEDIATE_S16: {
                        int16_t value;
                        if (recv(sock, (char *)&value, sizeof(value), 0) == SOCKET_ERROR) {
                            free(name);
                            lua_settop(scripting_api.lua_state, 0);
                            return result_error("InvalidIntermediateErr", "The Intermediate's variable value is unreadable.");
                        }
                        lua_pushnumber(scripting_api.lua_state, (double)value);
                        break;
                    }
                    case INTERMEDIATE_S32: {
                        int32_t value;
                        if (recv(sock, (char *)&value, sizeof(value), 0) == SOCKET_ERROR) {
                            free(name);
                            lua_settop(scripting_api.lua_state, 0);
                            return result_error("InvalidIntermediateErr", "The Intermediate's variable value is unreadable.");
                        }
                        lua_pushnumber(scripting_api.lua_state, (double)value);
                        break;
                    }
                    case INTERMEDIATE_S64: {
                        int64_t value;
                        if (recv(sock, (char *)&value, sizeof(value), 0) == SOCKET_ERROR) {
                            free(name);
                            lua_settop(scripting_api.lua_state, 0);
                            return result_error("InvalidIntermediateErr", "The Intermediate's variable value is unreadable.");
                        }
                        lua_pushnumber(scripting_api.lua_state, (double)value);
                        break;
                    }

                    case INTERMEDIATE_U8: {
                        uint8_t value;
                        if (recv(sock, (char *)&value, sizeof(value), 0) == SOCKET_ERROR) {
                            free(name);
                            lua_settop(scripting_api.lua_state, 0);
                            return result_error("InvalidIntermediateErr", "The Intermediate's variable value is unreadable.");
                        }
                        lua_pushnumber(scripting_api.lua_state, (double)value);
                        break;
                    }
                    case INTERMEDIATE_U16: {
                        uint16_t value;
                        if (recv(sock, (char *)&value, sizeof(value), 0) == SOCKET_ERROR) {
                            free(name);
                            lua_settop(scripting_api.lua_state, 0);
                            return result_error("InvalidIntermediateErr", "The Intermediate's variable value is unreadable.");
                        }
                        lua_pushnumber(scripting_api.lua_state, (double)value);
                        break;
                    }
                    case INTERMEDIATE_U32: {
                        uint32_t value;
                        if (recv(sock, (char *)&value, sizeof(value), 0) == SOCKET_ERROR) {
                            free(name);
                            lua_settop(scripting_api.lua_state, 0);
                            return result_error("InvalidIntermediateErr", "The Intermediate's variable value is unreadable.");
                        }
                        lua_pushnumber(scripting_api.lua_state, (double)value);
                        break;
                    }
                    case INTERMEDIATE_U64: {
                        uint64_t value;
                        if (recv(sock, (char *)&value, sizeof(value), 0) == SOCKET_ERROR) {
                            free(name);
                            lua_settop(scripting_api.lua_state, 0);
                            return result_error("InvalidIntermediateErr", "The Intermediate's variable value is unreadable.");
                        }
                        lua_pushnumber(scripting_api.lua_state, (double)value);
                        break;
                    }

                    case INTERMEDIATE_F32: {
                        float value;
                        if (recv(sock, (char *)&value, sizeof(value), 0) == SOCKET_ERROR) {
                            free(name);
                            lua_settop(scripting_api.lua_state, 0);
                            return result_error("InvalidIntermediateErr", "The Intermediate's variable value is unreadable.");
                        }
                        lua_pushnumber(scripting_api.lua_state, (double)value);
                        break;
                    }
                    case INTERMEDIATE_F64: {
                        double value;
                        if (recv(sock, (char *)&value, sizeof(value), 0) == SOCKET_ERROR) {
                            free(name);
                            lua_settop(scripting_api.lua_state, 0);
                            return result_error("InvalidIntermediateErr", "The Intermediate's variable value is unreadable.");
                        }
                        lua_pushnumber(scripting_api.lua_state, (double)value);
                        break;
                    }
                }

                lua_setfield(scripting_api.lua_state, -2, name);
                last_control = INTERMEDIATE_VARIABLE;
                free(name);
                break;

            case INTERMEDIATE_END:
                lua_getglobal(scripting_api.lua_state, "ds");
                lua_getfield(scripting_api.lua_state, -1, "events");
                lua_getfield(scripting_api.lua_state, -1, event_name);
                if (!lua_isfunction(scripting_api.lua_state, -1)) {
                    lua_settop(scripting_api.lua_state, 0);
                    return result_error("EventNotFoundErr", "The event type '%s' was not found.");
                }
                lua_pushvalue(scripting_api.lua_state, -4);
                if (lua_pcall(scripting_api.lua_state, 1, 1, 0) != LUA_OK) {
                    const char *str = lua_tostring(scripting_api.lua_state, -1);
                    lua_settop(scripting_api.lua_state, 0);
                    return result_error("LuaEventErr", str);
                }
                processing = false;
                break;

            default: {
                lua_settop(scripting_api.lua_state, 0);
                return result_error("UnknownControlErr", "Invalid control code encountered in event.");
            }
        }
    }
}