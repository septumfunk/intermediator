#include "socket.h"
#include "../data/stringext.h"
#include "../io/console.h"
#include <stdlib.h>

bool winsock_init(void) {
    WSADATA wsaData;
    int wsaerr;
    WORD wVersionRequested = MAKEWORD(2, 2);
    wsaerr = WSAStartup(wVersionRequested, &wsaData);

    if (wsaerr != 0) {
        console_error("Winsock DLL not found!");
        return false;
    }

    return true;
}

void winsock_cleanup(void) {
    WSACleanup();
}

void winsock_console_error(void) {
    wchar_t *s = nullptr;
    FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        WSAGetLastError(),
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPWSTR)&s,
        0,
        nullptr
    );
    s[wcslen(s) - 1] = 0;
    console_error("%S", s);
    LocalFree(s);
}

char *address_string(struct sockaddr_in address) {
    return format("%s:%d", inet_ntoa(address.sin_addr), ntohs(address.sin_port));
}

result_t socket_read_string(SOCKET sock, uint64_t max, char **out) {
    uint64_t size = 1;
    *out = calloc(1, sizeof(char));

    while (true) {
        char c;
        if (recv(sock, &c, sizeof(char), 0) == SOCKET_ERROR) {
            free(*out);
            return result_error("Socket encountered an error while reading a string.");
        }

        if (c == '\0') {
            return result_ok();
        } else if (size >= max) {
            free(*out);
            return result_error("String was longer than max.");
        }

        size++;
        *out = realloc(out, size);
        memcpy(*out + 1, *out, size - 1);
        *out[0] = c;
    }
}