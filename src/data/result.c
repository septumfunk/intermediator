#include "result.h"
#include "../data/stringext.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

result_t result_error(const char *description, ...) {
    va_list arglist;

    va_start(arglist, description);
    unsigned long long size =
        (unsigned long long)vsnprintf(nullptr, 0, description, arglist);
    va_end(arglist);

    char *text = calloc(1, size + 1);
    va_start(arglist, description);
    vsnprintf(text, size + 1, description, arglist);
    va_end(arglist);

    return (result_t) {
        .is_ok = false,
        .description = text,
    };
}

result_t result_ok(void) {
    return (result_t) {
        .is_ok = true,
        .description = nullptr,
    };
}

void result_discard(result_t result) {
    free(result.description);
}