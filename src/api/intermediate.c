#include "intermediate.h"
#include "../util/win32.h"
#include <float.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const static char *INTERNAL_VARIABLES[] = {
    "client",
    "reply",
    "id",
    "type",
};

intermediate_t *intermediate_new(char *event, uint32_t reply) {
    intermediate_t *intermediate = calloc(1, sizeof(intermediate_t));
    intermediate->id = intermediate_generate_id();
    intermediate->version = INTERMEDIATE_VERSION;
    intermediate->type = _strdup(event);
    intermediate->reply = reply;

    return intermediate;
}

void intermediate_delete(intermediate_t *self) {
    intermediate_variable_t *var = self->variables;
    while (var) {
        free(var->name);
        free(var->value);

        intermediate_variable_t *v = var;
        var = v->next;
        free(v);
    }

    free(self->type);
    free(self);
}

databuffer_t intermediate_serialize(intermediate_t *self) {
    /*char *buffer, *head;
    *len = sizeof(char) + sizeof(float) + sizeof(uint32_t) * 2 + strlen(self->type) + 2; // INTERMEDIATE_HEADER, header data, INTERMEDIATE_END
    head = buffer = calloc(1, *len);

    *head = (char)INTERMEDIATE_HEADER;
    head += sizeof(char);

    *(float *)head = INTERMEDIATE_VERSION;
    head += sizeof(float);

    *(uint32_t *)head = self->id;
    head += sizeof(uint32_t);
    *(uint32_t *)head = self->reply;
    head += sizeof(uint32_t);

    strcpy(head, self->type);
    head += strlen(self->type) + 1;

    for (intermediate_variable_t *var = self->variables; var; var = var->next) {
        bool internal = false;
        for (unsigned long long i = 0; i < sizeof(INTERNAL_VARIABLES) / sizeof(const char *); ++i) {
            if (strcmp(INTERNAL_VARIABLES[i], var->name) == 0) {
                internal = true;
                break;
            }
        }

        if (internal)
            continue;

        *len += sizeof(char); // intermediate_control_e
        *len += strlen(var->name) + 1;
        *len += sizeof(char); // intermediate_type_e

        int value_size;
        switch (var->type) {
            case INTERMEDIATE_STRING:
                value_size = strlen(var->value) + 1;
                break;

            case INTERMEDIATE_S8:
            case INTERMEDIATE_U8:
                value_size = sizeof(int8_t);
                break;

            case INTERMEDIATE_S16:
            case INTERMEDIATE_U16:
                value_size = sizeof(int16_t);
                break;

            case INTERMEDIATE_S32:
            case INTERMEDIATE_U32:
            case INTERMEDIATE_F32:
                value_size = sizeof(int32_t);
                break;

            case INTERMEDIATE_S64:
            case INTERMEDIATE_U64:
            case INTERMEDIATE_F64:
                value_size = sizeof(int64_t);
                break;
        }
        *len += value_size;

        // Realloc and write
        int prog = head - buffer;
        buffer = realloc(buffer, *len);
        head = buffer + prog;

        *head = (char)INTERMEDIATE_VARIABLE;
        head += sizeof(char);

        strcpy(head, var->name);
        head += strlen(head) + 1;

        *head = (char)var->type;
        head += sizeof(char);

        memcpy(head, var->value, value_size);
        head += value_size;
    }

    buffer[*len - 1] = INTERMEDIATE_END;

    return buffer;*/
}

result_t intermediate_deserialize(databuffer_t *buffer, intermediate_t **out) {
    result_t result = result_ok();

    intermediate_t *intermediate = calloc(1, sizeof(intermediate_t));
    intermediate->id = intermediate_generate_id();
    *out = intermediate;

    intermediate_table_t *current_table = nullptr;

    intermediate_control_e control = INTERMEDIATE_NONE;
    databuffer_seek(buffer, DATABUFFER_START, 0);
    while (databuffer_left(buffer)) {
        switch (control) {
            case INTERMEDIATE_NONE:
                if (databuffer_peek(buffer) != INTERMEDIATE_HEADER)
                    databuffer_step(buffer, 1);

                control = INTERMEDIATE_HEADER;
                databuffer_step(buffer, 1);
                break;
            case INTERMEDIATE_HEADER:
                if (!databuffer_read(buffer, (uint8_t *)&intermediate->version, sizeof(float)) ||
                    !databuffer_read_string(buffer, &intermediate->type, MAX_INTERMEDIATE_STRING_LENGTH)) {
                    result = result_error("Intermediate Header data is malformed.");
                    goto cleanup;
                }

                if (databuffer_peek(buffer) != INTERMEDIATE_TABLE) {
                    result = result_error("Unable to locate root table after header.");
                    goto cleanup;
                }

                control = INTERMEDIATE_TABLE;
                break;
            case INTERMEDIATE_TABLE:
                if (databuffer_peek(buffer) == INTERMEDIATE_TABLE) {
                    intermediate_table_t *new_table = calloc(1, sizeof(intermediate_table_t));
                    if (!databuffer_read_string(buffer, &new_table->name, MAX_INTERMEDIATE_STRING_LENGTH)) {
                        result = result_error("Unable to read table's name.");
                        goto cleanup;
                    }

                    if (current_table) {
                        new_table->parent = current_table;
                        new_table->siblings = current_table->children;
                        current_table->children = new_table;
                    } else {
                        intermediate->root_table = new_table;
                        current_table = new_table;
                    }

                    databuffer_step(buffer, 1);
                }

                switch (databuffer_peek(buffer)) {
                    case INTERMEDIATE_TABLE:
                        break;
                    case INTERMEDIATE_VARIABLE:
                        intermediate_variable_t *var = nullptr;
                        if (!(result = intermediate_variable_from_buffer(buffer, &var)).is_ok || !var)
                            goto cleanup;

                        var->next = current_table->variables;
                        current_table->variables = var;
                        break;
                    case INTERMEDIATE_ENDTABLE:
                        if (current_table->parent)
                            current_table = current_table->parent;
                        databuffer_step(buffer, 1);
                        break;
                }
                break;
            case INTERMEDIATE_END:
            default:
                result = result_error("Unexpected intermediate control code %d", control);
                goto cleanup;
                break;
        }
        if (!result.is_ok)
            goto cleanup;
    }

    if (control != INTERMEDIATE_END)
        result = result_error("Incomplete intermediate data.");

cleanup:
    if (!result.is_ok) {
        intermediate_delete(intermediate);
        databuffer_print(buffer);
    } else *out = intermediate;

    return result;
}

result_t intermediate_variable_from_buffer(databuffer_t *buffer, intermediate_variable_t **out) {
    if (databuffer_peek(buffer) != INTERMEDIATE_VARIABLE)
        return result_error("No variable header found.");
    databuffer_step(buffer, 1);

    auto result = result_ok();
    intermediate_variable_t *var = calloc(1, sizeof(intermediate_variable_t));
    if (databuffer_read_string(buffer, &var->name, MAX_INTERMEDIATE_STRING_LENGTH) < 1) {
        result = result_error("Variable name failed to read.");
        goto cleanup;
    }

    uint8_t type;
    if (databuffer_read(buffer, &type, sizeof(type)) != sizeof(type)) {
        result = result_error("Variable type failed to read.");
        goto cleanup;
    }
    var->type = type;

    if (var->type == INTERMEDIATE_STRING) {
        char *str = nullptr;
        if (!databuffer_read_string(buffer, &str, MAX_INTERMEDIATE_STRING_LENGTH) || !str) {
            result = result_error("Failed to read string value from variable '%s'.", var->name);
            goto cleanup;
        }
        var->value = str;
        goto cleanup;
    }

    auto size = intermediate_type_size(var->type);
    var->value = calloc(1, size);
    if (databuffer_read(buffer, var->value, size) != size) {
        result = result_error("Unable to read variable '%s' value.", var->name);
        goto cleanup;
    }

cleanup:
    if (!result.is_ok) {
        free(var->value);
        free(var->name);
        free(var);
    }
    return result;
}

void intermediate_add_var(intermediate_t *self, char *name, intermediate_type_e type, void *data, int size) {
    intermediate_variable_t *var = calloc(1, sizeof(intermediate_variable_t));
    var->name = _strdup(name);
    var->type = type;

    var->value = malloc(size);
    memcpy(var->value, data, size);

    var->next = self->variables;
    self->variables = var;
}

void intermediate_auto_number_var(intermediate_t *self, char *name, double number) {
    intermediate_variable_t *var = calloc(1, sizeof(intermediate_variable_t));

    var->name = _strdup(name);

    bool is_float = number - floor(number) != 0;

    if (is_float) {
        if (number > FLT_MAX) {
            var->type = INTERMEDIATE_F64;
            var->value = malloc(sizeof(double));
            *(double *)var->value = number;
        } else {
            var->type = INTERMEDIATE_F32;
            var->value = malloc(sizeof(float));
            *(float *)var->value = number;
        }
    } else {
        if (number > 0) {
            if (number < UINT8_MAX) {
                var->type = INTERMEDIATE_U8;
                var->value = malloc(sizeof(uint8_t));
                *(uint8_t *)var->value = number;
            } if (number > UINT8_MAX && number < UINT16_MAX) {
                var->type = INTERMEDIATE_U16;
                var->value = malloc(sizeof(uint16_t));
                *(uint16_t *)var->value = number;
            } else if (number > UINT16_MAX && number < UINT32_MAX) {
                var->type = INTERMEDIATE_U32;
                var->value = malloc(sizeof(uint32_t));
                *(uint32_t *)var->value = number;
            } else {
                var->type = INTERMEDIATE_U64;
                var->value = malloc(sizeof(uint64_t));
                *(uint64_t *)var->value = number;
            }
        } else {
            if (number < INT8_MIN) {
                var->type = INTERMEDIATE_S8;
                var->value = malloc(sizeof(int8_t));
                *(int8_t *)var->value = number;
            } if (number < INT8_MIN && number > INT16_MIN) {
                var->type = INTERMEDIATE_S16;
                var->value = malloc(sizeof(int16_t));
                *(int16_t *)var->value = number;
            } else if (number < INT16_MIN && number > INT32_MIN) {
                var->type = INTERMEDIATE_S32;
                var->value = malloc(sizeof(int32_t));
                *(int32_t *)var->value = number;
            } else {
                var->type = INTERMEDIATE_S64;
                var->value = malloc(sizeof(int64_t));
                *(int64_t *)var->value = number;
            }
        }
    }

    var->next = self->variables;
    self->variables = var;
}

uint32_t intermediate_generate_id(void) {
    uint32_t uuid = 0;
    if (BCryptGenRandom(nullptr, (unsigned char *)&uuid, sizeof(uuid), BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0)
        return max(((uint32_t)rand() << 16) | (uint32_t)rand(), 1);
    return uuid;
}

uint64_t intermediate_type_size(intermediate_type_e type) {
    switch (type) {
        case INTERMEDIATE_STRING: return 0;
        case INTERMEDIATE_BOOL: return sizeof(bool);

        case INTERMEDIATE_S8: return sizeof(int8_t);
        case INTERMEDIATE_S16: return sizeof(int16_t);
        case INTERMEDIATE_S32: return sizeof(int32_t);
        case INTERMEDIATE_S64: return sizeof(int64_t);

        case INTERMEDIATE_U8: return sizeof(uint8_t);
        case INTERMEDIATE_U16: return sizeof(uint16_t);
        case INTERMEDIATE_U32: return sizeof(uint32_t);
        case INTERMEDIATE_U64: return sizeof(uint64_t);

        case INTERMEDIATE_F32: return sizeof(float_t);
        case INTERMEDIATE_F64: return sizeof(double_t);
    }
}