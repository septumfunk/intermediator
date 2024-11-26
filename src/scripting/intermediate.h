#pragma once
#include "../structures/result.h"
#include <stdint.h>

#define INTERMEDIATE_VERSION 1.0f
#define MAX_INTERMEDIATE_SIZE 4096
#define MAX_INTERMEDIATE_STRING_LENGTH 512

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

typedef struct intermediate_variable_t {
    char *name;
    intermediate_type_e type;
    void *value;

    struct intermediate_variable_t *next;
} intermediate_variable_t;

typedef enum intermediate_control_e {
    INTERMEDIATE_NONE,
    INTERMEDIATE_HEADER,
    INTERMEDIATE_VARIABLE,
    INTERMEDIATE_END,
} intermediate_control_e;

typedef struct intermediate_t {
    float version;
    uint64_t id, reply;
    char *event;
    intermediate_variable_t *variables;

    struct intermediate_t *previous, *next;
} intermediate_t;

/// Insert an intermediate at the start of the list.
result_t intermediate_from_buffer(intermediate_t **out, char *buffer, int len);

/// Remove an intermediate from the end of the list, then return it.
intermediate_t *intermediates_pop(intermediate_t *list);

void intermediate_add_var(intermediate_t *self, char *name, intermediate_type_e type, void *data, int size);

void intermediate_delete(intermediate_t *self);