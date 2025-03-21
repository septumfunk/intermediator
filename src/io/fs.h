#pragma once
#include "../data/result.h"
#include <stdbool.h>

typedef unsigned long long fs_size_t;

/// Load file from path.
/// Out mustn't be allocated, it will be allocated automatically.
result_t fs_load(const char *path, char **out, fs_size_t *size);
/// Load checksummed file from path.
/// Out mustn't be allocated, it will be allocated automatically.
result_t fs_load_checksum(const char *path, char **out, fs_size_t *size);

/// Save file to path.
result_t fs_save(const char *path, const char *buffer, fs_size_t size);
/// Save checksummed file to path.
result_t fs_save_checksum(const char *path, const char *buffer, fs_size_t size);

/// Get the size of a file at path.
result_t fs_size(const char *path, fs_size_t *out);

/// Check if file exists.
bool fs_exists(const char *path);

/// Create a folder.
result_t fs_mkdir(const char *path);

/// Check if a folder exists.
bool fs_direxists(const char *path);

/// Execute a function with every file path in a directory.
void fs_recurse(const char *path, void (*callback)(const char *path, void *arg), void *arg);
