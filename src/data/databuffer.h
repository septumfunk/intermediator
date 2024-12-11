#pragma once
#include <stdint.h>

typedef struct databuffer_t {
    uint8_t *data, *head;
    uint64_t size;
} databuffer_t;

typedef enum databuffer_hook_e {
    DATABUFFER_START,
    DATABUFFER_END,
} databuffer_hook_e;

databuffer_t databuffer_create(void);
void databuffer_delete(databuffer_t *self);

void databuffer_resize(databuffer_t *self, uint64_t size);
void databuffer_clear(databuffer_t *self);

void databuffer_seek(databuffer_t *self, databuffer_hook_e hook, int64_t offset);
void databuffer_step(databuffer_t *self, int64_t offset);

void databuffer_write(databuffer_t *self, uint8_t *data, uint64_t size);
void databuffer_write_string(databuffer_t *self, const char *string);

uint64_t databuffer_read(databuffer_t *self, uint8_t *out, uint64_t bytes);
uint64_t databuffer_read_string(databuffer_t *self, char **out, uint64_t max_chars);

uint64_t databuffer_tell(databuffer_t *self);
uint64_t databuffer_left(databuffer_t *self);
uint8_t databuffer_peek(databuffer_t *self);
void databuffer_print(databuffer_t *self);