#include "databuffer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

databuffer_t databuffer_create(void) {
    return (databuffer_t) {
        .data = nullptr,
        .head = nullptr,
        .size = 0,
    };
}

void databuffer_delete(databuffer_t *self) {
    free(self->data);
    *self = databuffer_create();
}

void databuffer_resize(databuffer_t *self, uint64_t size) {
    self->size = size;
    self->data = realloc(self->data, self->size);
    
}
void databuffer_clear(databuffer_t *self);

void databuffer_seek(databuffer_t *self, databuffer_hook_e hook, int64_t offset) {
    if (!self->data) return;
    self->head = max(min(self->data + self->size, hook == DATABUFFER_START ? self->data + offset : self->data + self->size + offset), self->data);
}

void databuffer_step(databuffer_t *self, int64_t offset) {
    if (!self->data) return;
    if (!self->head) self->head = self->data;
    self->head = max(min(self->data + self->size, self->head + offset), self->data);
}

void databuffer_write(databuffer_t *self, uint8_t *data, uint64_t size) {
    if (!self->data) {
        self->data = calloc(1, size);
        self->head = self->data;
    } else {
        if (!self->head) self->head = self->data;
        if (self->head + size > self->data + self->size)
            self->data = realloc(self->data, (self->size += (self->head + size) - (self->data + self->size)));
    }

    memcpy(self->head, data, size);
    self->head += size;
}

void databuffer_write_string(databuffer_t *self, const char *string) {
    databuffer_write(self, (uint8_t *)string, strlen(string) + 1);
}

uint64_t databuffer_read(databuffer_t *self, uint8_t *out, uint64_t bytes) {
    if (!self->data) return 0;
    if (!self->head) self->head = self->data;
    memcpy(out, self->head, min(self->data + self->size - self->head, bytes));
    return min(self->data + self->size - self->head, bytes);
}

uint64_t databuffer_read_string(databuffer_t *self, char **out, uint64_t max_chars) {
    if (!self->data) return 0;
    if (!self->head) self->head = self->data;

    uint8_t *scan_head = self->head;
    uint64_t string_length = 1;
    while (scan_head < self->data + self->size && *scan_head && string_length < max_chars + 1) {
        string_length++;
        scan_head++;
    }
    scan_head++;

    *out = calloc(1, string_length);
    memcpy(*out, self->head, string_length - 1);

    return string_length - 1;
}

uint64_t databuffer_tell(databuffer_t *self) {
    return self->head - self->data;
}

uint64_t databuffer_left(databuffer_t *self) {
    return self->data + self->size - self->head;
}

uint8_t databuffer_peek(databuffer_t *self) {
    if (!self->data) return 0;
    if (!self->head) self->head = self->data;
    return *self->head;
}

void databuffer_print(databuffer_t *self) {
    databuffer_seek(self, DATABUFFER_START, 0);
    while (databuffer_left(self)) {
        if (!(databuffer_tell(self) % 16))
            printf("%llu | ", databuffer_tell(self) / 16 + 1);

        printf("%02x ", databuffer_peek(self));
        databuffer_step(self, 1);

        if (!databuffer_left(self) || !(databuffer_tell(self) % 16)) {
            auto size = min(self->head - self->data, 16);
            char *newstr = calloc(size +  1, sizeof(char));
            auto write = newstr;
            auto nhead = self->head - size;
            while (write < write + size) {
                if (*nhead < 127 && *nhead > 31)
                    *write = (char)*nhead;
                else *write = '-';
                write++;
                nhead++;
            }
            printf("%s\n", newstr);
            free(newstr);
        }
    }
}