#pragma once
#include "../win32/win32.h"
#include "../structures/result.h"

result_t socket_read_string(SOCKET sock, char **out, uint64_t max);