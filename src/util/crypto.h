#pragma once
#include <stdint.h>
#include <stdbool.h>

/// Jenkin's Hash Function
uint32_t jhash(const char *buffer, uint32_t size);
/// Jenkin's Hash Function compatible with a null terminated string
uint32_t jhash_str(const char *buffer);