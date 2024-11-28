#include "vector.h"
#include <stdlib.h>
#include <string.h>

vector_t vector_new(uint64_t element_size) {
    return (vector_t) {
        .data = NULL,
        .element_size = element_size,
        .count = 0,
    };
}

void vector_delete(vector_t *self) {
    free(self->data);
}

void *vector_insert(vector_t *self, void *data) {
    self->data = realloc(self->data, self->count + 1 * self->element_size);
    memcpy(self->data + self->count, data, self->element_size);
    self->count++;
    return self->data + (self->count - 1);
}

void *vector_get(vector_t *self, uint64_t index) {
    return self->data + index * self->element_size;
}

void vector_remove(vector_t *self, uint64_t index) {
    if (index >= self->count)
        return;

    if (index != self->count - 1)
        memcpy(self->data + index, self->data + index + 1, (self->count - index - 1) * self->element_size);
    self->data = realloc(self->data, self->count - 1 * self->element_size);
    self->count--;
}
