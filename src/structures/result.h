#pragma once
#include "../util/log.h"
#include <stdint.h>
#include <stdbool.h>

typedef struct result_t {
    bool is_error;
    char *type;
    char *description;
} result_t;

result_t result_error(const char *type, const char *description, ...);
result_t result_no_error(void);
void result_discard(result_t result);
bool result_match(result_t result, const char *type);

#define PANIC_FORMAT "In %s:%d:\n\n%s"
#define panic(result) {\
    if (result.is_error) {\
        log_error(result.type, PANIC_FORMAT, __FILE__, __LINE__, result.description);\
        result_discard(result);\
        exit(-1);\
    } else\
        result_discard(result);\
}