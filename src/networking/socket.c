#include "socket.h"
#include <stdlib.h>
#include <string.h>

result_t socket_read_string(SOCKET sock, char **out, uint64_t max) {
    uint64_t size = 1;
    *out = calloc(1, sizeof(char));

    while (true) {
        char c;
        if (recv(sock, &c, sizeof(char), 0) == SOCKET_ERROR) {
            free(*out);
            return result_error("SocketStringReadError", "Socket encountered an error while reading a string.");
        }

        if (c == '\0') {
            return result_no_error();
        } else if (size >= max) {
            free(*out);
            return result_error("SocketStringLengthError", "String was longer than max.");
        }

        size++;
        *out = realloc(out, size);
        memcpy(*out + 1, *out, size - 1);
        *out[0] = c;
    }
}