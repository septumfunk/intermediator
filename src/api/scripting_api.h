#pragma once
#include "intermediate.h"
#include "../data/result.h"
#include "../data/mutex.h"
#include <lua_all.h>
#include <winsock2.h>

#define DEFAULT_CONFIG "\
ds.config.tcp_port = 5060\n\
ds.config.udp_port = 5060\n\
ds.config.max_players = 512\
"

typedef struct scripting_api_t {
    lua_State *lua_state;
    mutex_t mutex;
} scripting_api_t;


result_t scripting_api_new(scripting_api_t *out);
void scripting_api_init_globals(scripting_api_t *self);
void scripting_api_cleanup(scripting_api_t *self);

void scripting_api_load_file(const char *name, scripting_api_t *self);

result_t scripting_api_config_number(scripting_api_t *self, const char *name, float *out, float def);
result_t scripting_api_config_string(scripting_api_t *self, const char *name, char **out, char *def);

void scripting_api_create_client(scripting_api_t *self, char *uuid, struct sockaddr_in addr);
void scripting_api_delete_client(scripting_api_t *self, char *uuid);

result_t scripting_api_try_event(scripting_api_t *self, intermediate_t *intermediate, char *uuid);