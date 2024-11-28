#include "intermediate.h"
#include <float.h>
#include <lua_all.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

intermediate_t *intermediate_new(char *event, uint32_t reply) {
    intermediate_t *inter = calloc(1, sizeof(intermediate_t));
    inter->version = INTERMEDIATE_VERSION;
    inter->event = _strdup(event);
    inter->reply = reply;

    return inter;
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

    free(self->event);

    if (self->previous)
        self->previous->next = self->next;
    if (self->next)
        self->next->previous = self->previous;

    free(self);
}

char *intermediate_to_buffer(intermediate_t *self, int *len) {
    char *buffer, *head;
    *len = sizeof(char) + sizeof(float) + sizeof(uint32_t) * 2 + strlen(self->event) + 2; // INTERMEDIATE_HEADER, header data, INTERMEDIATE_END
    head = buffer = calloc(1, *len);

    *head = (char)INTERMEDIATE_HEADER;
    head += sizeof(char);

    *(float *)head = INTERMEDIATE_VERSION;
    head += sizeof(float);

    *(uint32_t *)head = self->id;
    head += sizeof(uint32_t);
    *(uint32_t *)head = self->reply;
    head += sizeof(uint32_t);

    strcpy(head, self->event);
    head += strlen(self->event) + 1;

    for (intermediate_variable_t *var = self->variables; var; var = var->next) {
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

    return buffer;
}

result_t intermediate_from_buffer(intermediate_t **out, char *buffer, int len, char *uuid) {
    result_t res = result_no_error();
    intermediate_t *inter = calloc(1, sizeof(intermediate_t));
    intermediate_control_e cc = INTERMEDIATE_NONE;
    inter->client_uuid = _strdup(uuid);

    char *head = buffer;
    while (head < buffer + len && cc != INTERMEDIATE_END) {
        switch ((intermediate_control_e)*head) {
            case INTERMEDIATE_HEADER:
                head++;
                if (cc != INTERMEDIATE_NONE) {
                    res = result_error("IntermediateOrderErr", "Intermediate contents are out of order.");
                    goto cleanup;
                }

                if ((buffer + len) - head < (long long)sizeof(float)) {
                    res = result_error("IntermediateSizeErr", "Intermediate wasn't correctly sized.");
                    goto cleanup;
                }
                inter->version = *(float *)head;
                head += sizeof(float);

                if ((buffer + len) - head < (long long)sizeof(uint32_t)) {
                    res = result_error("IntermediateSizeErr", "Intermediate wasn't correctly sized.");
                    goto cleanup;
                }
                inter->id = *(uint32_t *)head;
                head += sizeof(uint32_t);

                if ((buffer + len) - head < (long long)sizeof(uint32_t)) {
                    res = result_error("IntermediateSizeErr", "Intermediate wasn't correctly sized.");
                    goto cleanup;
                }
                inter->reply = *(uint32_t *)head;
                head += sizeof(uint32_t);

                if (strlen(head) > MAX_INTERMEDIATE_STRING_LENGTH || (long long)strlen(head) > buffer + len - head) {
                    res = result_error("IntermediateSizeErr", "Intermediate wasn't correctly sized.");
                    goto cleanup;
                }
                inter->event = _strdup(head);
                head += strlen(head) + 1;

                cc = INTERMEDIATE_HEADER;
                break;

            case INTERMEDIATE_VARIABLE:
                head++;
                if (cc != INTERMEDIATE_HEADER && cc != INTERMEDIATE_VARIABLE) {
                    res = result_error("IntermediateOrderErr", "Intermediate contents are out of order.");
                    goto cleanup;
                }

                intermediate_variable_t *var = calloc(1, sizeof(intermediate_variable_t));
                if (strlen(head) > MAX_INTERMEDIATE_STRING_LENGTH || (long long)strlen(head) > buffer + len - head) {
                    free(var);
                    res = result_error("IntermediateSizeErr", "Intermediate wasn't correctly sized.");
                    goto cleanup;
                }
                var->name = _strdup(head);
                head += strlen(var->name) + 1;

                if ((buffer + len) - head < (long long)sizeof(char)) {
                    free(var);
                    res = result_error("IntermediateSizeErr", "Intermediate wasn't correctly sized.");
                    goto cleanup;
                }
                var->type = *head;
                head += sizeof(char);

                switch (var->type) {
                    case INTERMEDIATE_STRING:
                        if (strlen(head) > MAX_INTERMEDIATE_STRING_LENGTH || (long long)strlen(head) > buffer + len - head) {
                            free(var);
                            res = result_error("IntermediateSizeErr", "Intermediate wasn't correctly sized.");
                            goto cleanup;
                        }
                        var->value = _strdup(head);
                        head += strlen(head) + 1;
                        break;

                    case INTERMEDIATE_S8:
                    case INTERMEDIATE_U8:
                        if ((buffer + len) - head < (long long)sizeof(int8_t)) {
                            free(var);
                            res = result_error("IntermediateSizeErr", "Intermediate wasn't correctly sized.");
                            goto cleanup;
                        }
                        var->value = calloc(1, sizeof(int8_t));
                        memcpy(var->value, head, sizeof(int8_t));
                        head += sizeof(int8_t);
                        break;

                    case INTERMEDIATE_S16:
                    case INTERMEDIATE_U16:
                        if ((buffer + len) - head < (long long)sizeof(int16_t)) {
                            free(var);
                            res = result_error("IntermediateSizeErr", "Intermediate wasn't correctly sized.");
                            goto cleanup;
                        }
                        var->value = calloc(1, sizeof(int16_t));
                        memcpy(var->value, head, sizeof(int16_t));
                        head += sizeof(int16_t);
                        break;

                    case INTERMEDIATE_S32:
                    case INTERMEDIATE_U32:
                    case INTERMEDIATE_F32:
                        if ((buffer + len) - head < (long long)sizeof(int32_t)) {
                            free(var);
                            res = result_error("IntermediateSizeErr", "Intermediate wasn't correctly sized.");
                            goto cleanup;
                        }
                        var->value = calloc(1, sizeof(int32_t));
                        memcpy(var->value, head, sizeof(int32_t));
                        head += sizeof(int32_t);
                        break;

                    case INTERMEDIATE_S64:
                    case INTERMEDIATE_U64:
                    case INTERMEDIATE_F64:
                        if ((buffer + len) - head < (long long)sizeof(int64_t)) {
                            free(var);
                            res = result_error("IntermediateSizeErr", "Intermediate wasn't correctly sized.");
                            goto cleanup;
                        }
                        var->value = calloc(1, sizeof(int64_t));
                        memcpy(var->value, head, sizeof(int64_t));
                        head += sizeof(int64_t);
                        break;
                }

                var->next = inter->variables;
                inter->variables = var;
                cc = INTERMEDIATE_VARIABLE;
                break;

            case INTERMEDIATE_END:
                cc = INTERMEDIATE_END;
                break;

            default:
                return result_error("IntermediateOrderErr", "Intermediate contents are out of order.");
        }
    }

    if (inter->version != INTERMEDIATE_VERSION) {
        res = result_error("IntermediateVersionErr", "This server doesn't support intermediate version %f.", inter->version);
        goto cleanup;
    }

    inter->next = *out;
    if (*out)
        (*out)->previous = inter;
    *out = inter;
    return res;
cleanup:
    intermediate_delete(inter);
    return res;
}

intermediate_t *intermediates_pop(intermediate_t **list) {
    if (!*list) return NULL;
    intermediate_t *head = *list;
    while (head && head->next)
        head = head->next;

    if (head->previous)
        head->previous->next = head->next; // should be NULL
    head->previous = NULL;
    head->next = NULL;

    if (head == *list)
        *list = NULL;

    return head;
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
    return ((uint32_t)rand() << 16) | (uint32_t)rand();
}
