#pragma once
#include "../structures/result.h"
#include "../util/win.h"
#include <lua_all.h>

#define DEFAULT_CONFIG "\
ds.config.title = 'Default Server'\n\
ds.config.port_tcp = 6342\n\
ds.config.port_udp = 6342\n\
ds.config.max_players = 500\
"

typedef struct scripting_api_t {
    lua_State *lua_state;
} scripting_api_t;
extern scripting_api_t scripting_api;

typedef struct intermediate_header_t {
    uint8_t version;
    uint64_t id;
    uint64_t reply;
} intermediate_header_t;

typedef enum intermediate_version_e {
    INTERMEDIATE_VERSION_1_0,
} intermediate_version_e;

typedef enum intermediate_control_e {
    INTERMEDIATE_NONE,
    INTERMEDIATE_HEADER,
    INTERMEDIATE_VARIABLE,
    INTERMEDIATE_END,
} intermediate_control_e;

typedef enum intermediate_type_e {
    INTERMEDIATE_STRING,

    INTERMEDIATE_S8,
    INTERMEDIATE_S16,
    INTERMEDIATE_S32,
    INTERMEDIATE_S64,

    INTERMEDIATE_U8,
    INTERMEDIATE_U16,
    INTERMEDIATE_U32,
    INTERMEDIATE_U64,

    INTERMEDIATE_F32,
    INTERMEDIATE_F64,
} intermediate_type_e;

void scripting_api_init(void);
void scripting_api_init_globals(void);
void scripting_api_cleanup(void);

void scripting_api_load_file(const char *name);

result_t scripting_api_config_number(const char *name, float *out);
result_t scripting_api_config_string(const char *name, char **out);

result_t scripting_api_try_event(const char *event_name, SOCKET sock);