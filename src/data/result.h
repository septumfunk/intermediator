#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef struct result_t {
    bool is_ok;
    char *description;
} result_t;

result_t result_error(const char *description, ...);
result_t result_ok(void);
void result_discard(result_t result);
