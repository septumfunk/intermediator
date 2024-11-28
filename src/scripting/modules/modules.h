#pragma once
#include <lua_all.h>
#include <stdint.h>

#define module_function(module, function)

typedef struct scripting_function_t {
    const char *name;
    lua_CFunction function;
} scripting_function_t;

typedef struct scripting_module_t {
    const char *name;
    uint32_t function_count;
    scripting_function_t *functions;
} scripting_module_t;

typedef enum scripting_modules_e {
    SCRIPTING_MODULES_PACKETS,
    SCRIPTING_MODULES_CLIENTS,
    SCRIPTING_MODULES_COUNT,
} scripting_modules_e;

extern scripting_module_t scripting_modules[SCRIPTING_MODULES_COUNT];