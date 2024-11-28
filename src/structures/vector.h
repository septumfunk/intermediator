#pragma once
#include <stdint.h>

/// Vector of objects. Can be referenced as a regular array by casting its pointer (pointer points to start of data).
typedef struct vector_t {
    void *data;
    uint64_t count;
    uint64_t element_size;
} vector_t;

vector_t vector_new(uint64_t element_size);
void vector_delete(vector_t *self);

void *vector_insert(vector_t *self, void *data);
void *vector_get(vector_t *self, uint64_t index);
void vector_remove(vector_t *self, uint64_t index);