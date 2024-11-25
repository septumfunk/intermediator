#pragma once

#define ANSI_COLOR_BLACK   "\033[30m"
#define ANSI_COLOR_RED     "\033[31m"
#define ANSI_COLOR_GREEN   "\033[32m"
#define ANSI_COLOR_YELLOW  "\033[33m"
#define ANSI_COLOR_BLUE    "\033[34m"
#define ANSI_COLOR_MAGENTA "\033[35m"
#define ANSI_COLOR_CYAN    "\033[36m"
#define ANSI_COLOR_WHITE   "\033[37m"

#define ANSI_BG_BLACK      "\033[40m"
#define ANSI_BG_RED        "\033[41m"
#define ANSI_BG_GREEN      "\033[42m"
#define ANSI_BG_YELLOW     "\033[43m"
#define ANSI_BG_BLUE       "\033[44m"
#define ANSI_BG_MAGENTA    "\033[45m"
#define ANSI_BG_CYAN       "\033[46m"
#define ANSI_BG_WHITE      "\033[47m"

#define ANSI_STYLE_RESET     "\033[0m"
#define ANSI_STYLE_BOLD      "\033[1m"
#define ANSI_STYLE_UNDERLINE "\033[4m"

#define LOG_INFO_FORMAT "LOG | %s\n"
#define LOG_WARN_FORMAT "WRN | %s\n"
#define LOG_ERROR_FORMAT "ERR | %s\n"
#define LOG_HEADER_FORMAT "[%s]\n"

/// Log info in console.
void log_info(const char *format, ...);
/// Log warning in console.
void log_warn(const char *format, ...);
/// Log error in console.
void log_error(const char *format, ...);
/// Print a logging header to the console
void log_header(const char *format, ...);