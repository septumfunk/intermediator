#pragma once
#include "../util/win32.h"
#include "../data/result.h"

/// Initialize Winsock. Returns whether it succeeded or not.
bool winsock_init(void);
/// Clean up after Winsock
void winsock_cleanup(void);
/// Log the last winsock error to stderr.
void winsock_console_error(void);

/// Converts a sockaddr_in into a string representation and returns it.
char *address_string(struct sockaddr_in address);

result_t socket_read_string(SOCKET sock, uint64_t max, char **out);