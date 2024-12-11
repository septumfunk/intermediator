#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef struct result_t {
    bool is_ok;
    char *description;
} result_t;

#define result_error(description, ...) _result_error(description" @ %s:%d", __VA_ARGS__, __FILE__, __LINE__)
result_t _result_error(const char *description, ...);

result_t result_ok(void);
void result_discard(result_t result);
