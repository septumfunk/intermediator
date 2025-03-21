#pragma once
#include "../data/result.h"
#include <stdint.h>

#define INTERMEDIATE_VERSION 1.0f
#define MAX_INTERMEDIATE_SIZE 1024
#define MAX_INTERMEDIATE_STRING_LENGTH 512

extern const char *INTERNAL_VARIABLES[];

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
    uint32_t id, reply;
    char *type;
    intermediate_variable_t *variables;
} intermediate_t;

/// Create a default intermediate.
intermediate_t *intermediate_new(char *event, uint32_t reply);
void intermediate_delete(intermediate_t *self);

/// Convert an intermediate into a buffer.
char *intermediate_to_buffer(intermediate_t *self, int *len);
/// Insert an intermediate at the start of the list.
result_t intermediate_from_buffer(char *buffer, int len, intermediate_t **out);


void intermediate_add_var(intermediate_t *self, char *name, intermediate_type_e type, void *data, int size);
void intermediate_auto_number_var(intermediate_t *self, char *name, double number);

uint32_t intermediate_generate_id(void);