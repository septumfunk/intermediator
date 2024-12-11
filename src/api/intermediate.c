#include "intermediate.h"
#include "../util/win32.h"
#include "../data/stringext.h"
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
    auto table = self->root_table;
    while (table) {
        auto var = table->variables;
        while (var) {
            auto next = var->next;
            free(var->name);
            free(var->value);
            free(var);
            var = next;
        }
        table->variables = nullptr;

        if (table->children) {
            auto next = table->children;
            table->children = nullptr;
            table = next;
            continue;
        } else if (table->siblings) {
            auto next = table->siblings;
            free(table);
            table = next;
            continue;
        } else {
            auto next = table->parent;
            free(table);
            table = next;
            continue;
        }

        table = table->parent;
    }

    free(self->type);
    free(self);
}

databuffer_t intermediate_serialize(intermediate_t *self) {
    auto buffer = databuffer_create();

    uint8_t control = INTERMEDIATE_HEADER;
    databuffer_write(&buffer, &control, sizeof(uint8_t));
    databuffer_write(&buffer, (uint8_t *)&self->version, sizeof(float_t));
    databuffer_write_string(&buffer, self->type);

    auto current_table = self->root_table;
    while (current_table) {
        control = INTERMEDIATE_TABLE;
        databuffer_write(&buffer, &control, sizeof(uint8_t));
        databuffer_write_string(&buffer, current_table->name);

        control = INTERMEDIATE_VARIABLE;
        auto var = current_table->variables;
        while (var) {
            databuffer_write(&buffer, &control, sizeof(uint8_t));
            databuffer_write_string(&buffer, var->name);

            uint8_t type = var->type;
            databuffer_write(&buffer, &type, sizeof(uint8_t));

            if (type == INTERMEDIATE_STRING)
                databuffer_write_string(&buffer, var->value);
            else
                databuffer_write(&buffer, var->value, intermediate_type_size(var->type));

            var = var->next;
        }

        if (current_table->children) {
            current_table = current_table->children;
            continue;
        }

        control = INTERMEDIATE_ENDTABLE;
        databuffer_write(&buffer, &control, sizeof(uint8_t));

        auto next_table = current_table;
        while (next_table) {
            if (next_table->siblings) {
                next_table = next_table->siblings;
            } else if (next_table->parent) {
                control = INTERMEDIATE_ENDTABLE;
                databuffer_write(&buffer, &control, sizeof(uint8_t));
                next_table = next_table->parent;
            }
        }
    }

    control = INTERMEDIATE_END;
    databuffer_write(&buffer, &control, sizeof(uint8_t));

    return buffer;
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

intermediate_table_t *intermediate_get_child(intermediate_table_t *table, char *name) {
    auto head = table->children;
    while (head) {
        if (bstrcmp(head->name, name))
            return head;
        head = head->siblings;
    }

    return nullptr;
}

intermediate_table_t *intermediate_add_child(intermediate_table_t *table, char *name) {
    intermediate_table_t *child = calloc(1, sizeof(intermediate_table_t));
    child->name = _strdup(name);
    child->siblings = table->children;
    table->children = child;
}

intermediate_variable_t *intermediate_add_variable(intermediate_table_t *table, char *name, intermediate_type_e type, void *value) {
    intermediate_variable_t *var = calloc(1, sizeof(intermediate_variable_t));
    var->name = _strdup(name);
    var->type = type;

    if (type == INTERMEDIATE_STRING)
        var->value = _strdup(value);
    else {
        auto size = intermediate_type_size(type);
        var->value = calloc(1, size);
        memcpy(var->value, value, size);
    }

    var->next = table->variables;
    table->variables = var;

    return var;
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

uint64_t intermediate_number_convert(double number, intermediate_type_e *type, uint8_t **buffer) {
    bool is_float = number - floor(number) == 0;
    if (is_float) {
        if (number > FLT_MAX || number < FLT_MIN)
            *type = INTERMEDIATE_F64;
        else
            *type = INTERMEDIATE_F32;
    } else if (number >= 0) { // Unsigned
        if (number > UINT32_MAX)
            *type = INTERMEDIATE_U64;
        else if (number > UINT16_MAX)
            *type = INTERMEDIATE_U32;
        else if (number > UINT8_MAX)
            *type = INTERMEDIATE_U16;
        else
            *type = INTERMEDIATE_U8;
    } else { // Signed
        if (number < INT32_MIN)
            *type = INTERMEDIATE_U64;
        else if (number < INT16_MIN)
            *type = INTERMEDIATE_S32;
        else if (number < INT8_MIN)
            *type = INTERMEDIATE_S16;
        else
            *type = INTERMEDIATE_S8;
    }

    auto size = intermediate_type_size(*type);
    *buffer = calloc(1, size);

    switch (*type) {
        case INTERMEDIATE_F32: { auto val = (float_t)number; memcpy(*buffer, &val, size); break; }
        case INTERMEDIATE_F64: { auto val = (double_t)number; memcpy(*buffer, &val, size); break; }

        case INTERMEDIATE_U8: { auto val = (uint8_t)number; memcpy(*buffer, &val, size); break; }
        case INTERMEDIATE_U16: { auto val = (uint16_t)number; memcpy(*buffer, &val, size); break; }
        case INTERMEDIATE_U32: { auto val = (uint32_t)number; memcpy(*buffer, &val, size); break; }
        case INTERMEDIATE_U64: { auto val = (uint64_t)number; memcpy(*buffer, &val, size); break; }

        case INTERMEDIATE_S8: { auto val = (int8_t)number; memcpy(*buffer, &val, size); break; }
        case INTERMEDIATE_S16: { auto val = (int16_t)number; memcpy(*buffer, &val, size); break; }
        case INTERMEDIATE_S32: { auto val = (int32_t)number; memcpy(*buffer, &val, size); break; }
        case INTERMEDIATE_S64: { auto val = (int64_t)number; memcpy(*buffer, &val, size); break; }

        default: { free(buffer); return 0; }
    }

    return size;
}