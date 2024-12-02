#include "console.h"
#include "../util/win32.h"
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

void console_init(void) {
    SetConsoleMode(GetConsoleWindow(), ENABLE_VIRTUAL_TERMINAL_PROCESSING);
}

void console_log(const char *format, ...) {
    va_list arglist;

    va_start(arglist, format);
    unsigned long long size = (unsigned long long)vsnprintf(nullptr, 0, format, arglist);
    va_end(arglist);

    char *text = calloc(1, size + 1);
    va_start(arglist, format);
    vsnprintf(text, size + 1, format, arglist);
    va_end(arglist);

    char timestr[64];
    time_t _time = time(nullptr);
    strftime(timestr, sizeof(timestr), "%m/%d/%Y %I:%M:%S", localtime(&_time));

    printf(LOG_INFO_FORMAT, timestr, text);
    free(text);
}

void console_warn(const char *format, ...) {
    va_list arglist;

    va_start(arglist, format);
    unsigned long long size = (unsigned long long)vsnprintf(nullptr, 0, format, arglist);
    va_end(arglist);

    char *text = calloc(1, size + 1);
    va_start(arglist, format);
    vsnprintf(text, size + 1, format, arglist);
    va_end(arglist);

    char timestr[64];
    time_t _time = time(nullptr);
    strftime(timestr, sizeof(timestr), "%m/%d/%Y %I:%M:%S", localtime(&_time));

    printf(LOG_WARN_FORMAT, timestr, text);
    free(text);
}

void console_error(const char *format, ...) {
    va_list arglist;

    va_start(arglist, format);
    unsigned long long size = (unsigned long long)vsnprintf(nullptr, 0, format, arglist);
    va_end(arglist);

    char *text = calloc(1, size + 1);
    va_start(arglist, format);
    vsnprintf(text, size + 1, format, arglist);
    va_end(arglist);

    char timestr[64];
    time_t _time = time(nullptr);
    strftime(timestr, sizeof(timestr), "%m/%d/%Y %I:%M:%S", localtime(&_time));

    printf(LOG_ERROR_FORMAT, timestr, text);
    free(text);
}

void console_header(const char *format, ...) {
    va_list arglist;

    va_start(arglist, format);
    unsigned long long size = (unsigned long long)vsnprintf(nullptr, 0, format, arglist);
    va_end(arglist);

    char *text = calloc(1, size + 1);
    va_start(arglist, format);
    vsnprintf(text, size + 1, format, arglist);
    va_end(arglist);

    char timestr[64];
    time_t _time = time(nullptr);
    strftime(timestr, sizeof(timestr), "%m/%d/%Y %I:%M:%S", localtime(&_time));

    fprintf(stderr, LOG_HEADER_FORMAT, timestr, text);
    free(text);
}
