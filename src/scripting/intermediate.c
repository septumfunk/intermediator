#include "intermediate.h"
#include <lua_all.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

result_t intermediate_from_buffer(intermediate_t **out, char *buffer, int len) {
    result_t res = result_no_error();
    intermediate_t *inter = calloc(1, sizeof(intermediate_t));
    intermediate_control_e cc = INTERMEDIATE_NONE;

    for (char *head = buffer; head < buffer + len; ++head) {
        switch (*head) {
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

                if ((buffer + len) - head < (long long)sizeof(uint64_t)) {
                    res = result_error("IntermediateSizeErr", "Intermediate wasn't correctly sized.");
                    goto cleanup;
                }
                inter->id = *(uint64_t *)head;
                head += sizeof(uint64_t);

                if ((buffer + len) - head < (long long)sizeof(uint64_t)) {
                    res = result_error("IntermediateSizeErr", "Intermediate wasn't correctly sized.");
                    goto cleanup;
                }
                inter->reply = *(uint64_t *)head;
                head += sizeof(uint64_t);

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

                cc = INTERMEDIATE_VARIABLE;
                break;

            case INTERMEDIATE_END:
                cc = INTERMEDIATE_END;
                break;
        }
    }

    if (inter->version != INTERMEDIATE_VERSION) {
        res = result_error("IntermediateVersionErr", "This server doesn't support intermediate version %f.", INTERMEDIATE_VERSION);
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

intermediate_t *intermediates_pop(intermediate_t *list) {
    while (list && list->next)
        list = list->next;
    if (!list) return NULL;

    if (list->previous)
        list->previous->next = list->next; // should be NULL
    list->previous = NULL;
    list->next = NULL;

    return list;
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
