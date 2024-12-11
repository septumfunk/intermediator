#pragma once
#include "../data/result.h"
#include "../data/databuffer.h"
#include <stdint.h>

#define INTERMEDIATE_VERSION 1.1f
#define MAX_INTERMEDIATE_SIZE 1024
#define MAX_INTERMEDIATE_STRING_LENGTH 512

extern const char *INTERNAL_VARIABLES[];

typedef enum intermediate_type_e {
    INTERMEDIATE_STRING,
    INTERMEDIATE_BOOL,

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

typedef struct intermediate_table_t {
    char *name;
    struct intermediate_table_t *siblings;
    struct intermediate_table_t *parent, *children;
    intermediate_variable_t *variables;
} intermediate_table_t;

typedef enum intermediate_control_e {
    INTERMEDIATE_NONE,
    INTERMEDIATE_HEADER,
    INTERMEDIATE_TABLE,
    INTERMEDIATE_VARIABLE,
    INTERMEDIATE_ENDTABLE,
    INTERMEDIATE_END,
} intermediate_control_e;

typedef struct intermediate_t {
    float version;
    uint32_t id, reply;
    char *type;
    intermediate_table_t *root_table;
} intermediate_t;

/// Create a default intermediate.
intermediate_t *intermediate_new(char *event, uint32_t reply);
void intermediate_delete(intermediate_t *self);

/// Convert an intermediate into a buffer.
databuffer_t intermediate_serialize(intermediate_t *self);
/// Insert an intermediate at the start of the list.
result_t intermediate_deserialize(databuffer_t *buffer, intermediate_t **out);

result_t intermediate_variable_from_buffer(databuffer_t *buffer, intermediate_variable_t **out);
intermediate_variable_t intermediate_number_variable(char *name, double number);

void intermediate_get_child(intermediate_table_t *table, char *name);
void intermediate_add_table(intermediate_t *self, char *name);
void intermediate_add_variable(intermediate_table_t *table, char *name, intermediate_type_e type, uint8_t *value);

uint32_t intermediate_generate_id(void);
uint64_t intermediate_type_size(intermediate_type_e type);