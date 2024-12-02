#include "fs.h"
#include "../data/result.h"
#include "../data/stringext.h"
#include "../util/win32.h"
#include <fileapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

result_t fs_load(const char *path, char **out, fs_size_t *size) {
    if (!fs_exists(path))
        return result_error("Requested file '%s' could not be found.", path);
    FILE *f = fopen(path, "rb");
    if (!f)
        return result_error("Call to fopen on requested file '%s' failed.", path);

    // Stat size
    fseek(f, 0, SEEK_END);
    *size = (fs_size_t)ftell(f);
    fseek(f, 0, SEEK_SET);

    // Allocate buffer
    *out = calloc(1, *size + 1);

    // Read into buffer
    if (fread(*out, *size, 1, f) < 0)
        return result_error("The requested file '%s' was unable to be read.", path);
    fclose(f);

    return result_ok();
}


result_t fs_save(const char *path, const char *buffer, fs_size_t size) {
    FILE *f = fopen(path, "wb");
    if (!f)
        return result_error("Call to fopen on requested file '%s' failed.", path);

    if (fwrite(buffer, size, 1, f) < 0)
        return result_error("The requested file '%s' was unable to be written to.", path);
    fclose(f);

    return result_ok();
}

result_t fs_size(const char *path, fs_size_t *out) {
    FILE *f = fopen(path, "rb");
    if (!f)
        return result_error("Call to fopen on requested file '%s' failed.", path);

    fseek(f, 0, SEEK_END);
    *out = (fs_size_t)ftell(f);
    fclose(f);

    return result_ok();
}

bool fs_exists(const char *path) {
    fs_size_t size;
    if (!fs_size(path, &size).is_ok || size < 0)
        return false;
    return true;
}

result_t fs_mkdir(const char *path) {
    if (!CreateDirectory(path, nullptr))
        return result_error("Failed to create folder '%s'.", path);
    return result_ok();
}

bool fs_direxists(const char *path) {
    DWORD dwAttrib = GetFileAttributes(path);
    return (dwAttrib != INVALID_FILE_ATTRIBUTES && (dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

void fs_recurse(const char *path, void (*callback)(const char *path, void *arg), void *arg) {
    WIN32_FIND_DATA data;

    char *scan = format("%s\\*", path);
    HANDLE hFind = FindFirstFile(scan, &data);
    while (hFind != INVALID_HANDLE_VALUE) {
        if (strcmp(data.cFileName, ".") != 0 && strcmp(data.cFileName, "..") != 0) {
            char *p = format("%s\\%s", path, data.cFileName);
            if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                fs_recurse(p, callback, arg);
            else
                callback(p, arg);
            free(p);
        }

        if (!FindNextFile(hFind, &data))
            break;
    }
    FindClose(hFind);
}