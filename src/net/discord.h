#pragma once
#include "../data/result.h"
#include <stdint.h>

#define DISCORD_API_ENDPOINT "https://discord.com/api/v10"

typedef uint64_t discord_id_t;

result_t discord_info_from_code(const char *code, discord_id_t *out, const char **out2);