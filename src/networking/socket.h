#pragma once
#include "../util/win.h"
#include "../structures/result.h"

result_t socket_read_string(SOCKET sock, char **out, uint64_t max);