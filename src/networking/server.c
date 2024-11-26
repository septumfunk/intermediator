#include "server.h"
#include "../util/win.h"
#include "../util/log.h"
#include "../scripting/scripting_api.h"
#include <math.h>
#include <minwinbase.h>
#include <stdlib.h>
#include <winsock2.h>

server_t server;

void server_start(void) {
    result_t res;
    if ((res = scripting_api_new(&server.api)).is_error) {
        log_error(res.description);
        result_discard(res);
        server_stop();
    }
    server.events = NULL;

    log_header("Initializing Server");
    server_init_winsock();

    server.tcp_socket = INVALID_SOCKET;
    if ((server.tcp_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == INVALID_SOCKET) {
        log_error("Failed to open TCP socket.");
        server_stop();
    }
    log_info("Opened TCP socket.");
    server.tcp_addr.sin_family = AF_INET;
    server.tcp_addr.sin_addr.s_addr = inet_addr("0.0.0.0");

    float port;
    if ((res = scripting_api_config_number(&server.api, "tcp_port", &port)).is_error) {
        log_error(res.description);
        result_discard(res);
        server_stop();
    }
    server.tcp_addr.sin_port = htons((u_short)floorf(port));

    if (bind(server.tcp_socket, (struct sockaddr *)&server.tcp_addr, sizeof(struct sockaddr)) == SOCKET_ERROR) {
        log_error("Failed to bind TCP socket.");
        server_stop();
    }

    server.udp_socket = INVALID_SOCKET;
    if ((server.udp_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == INVALID_SOCKET) {
        log_error("Failed to open UDP socket.");
        server_stop();
    }
    log_info("Opened UDP socket.");
    server.udp_addr.sin_family = AF_INET;
    server.udp_addr.sin_addr.s_addr = inet_addr("0.0.0.0");

    if ((res = scripting_api_config_number(&server.api, "udp_port", &port)).is_error) {
        log_error(res.description);
        result_discard(res);
        server_stop();
    }
    server.udp_addr.sin_port = htons((u_short)floorf(port));

    if (bind(server.udp_socket, (struct sockaddr *)&server.udp_addr, sizeof(struct sockaddr)) == SOCKET_ERROR) {
        log_error("Failed to bind UDP socket.");
        server_stop();
    }
    server.udp_thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)server_listen_udp, NULL, 0, server.udp_id);

    server_listen_tcp();
}

void server_init_winsock(void) {
    WSADATA wsaData;
    int wsaerr;
    WORD wVersionRequested = MAKEWORD(2, 2);
    wsaerr = WSAStartup(wVersionRequested, &wsaData);

    if (wsaerr != 0) {
        log_error("Winsock DLL not found!");
        server_stop();
    }
    log_info("Winsock initialized!");
}

void server_stop(void) {
    WSACleanup();
    exit(-1);
}

void server_listen_tcp(void) {
    if (listen(server.tcp_socket, 6) == SOCKET_ERROR) {
        log_error("Failed to listen on TCP socket.");
        server_stop();
    }
    log_info("Listening on TCP port %d.", ntohs(server.tcp_addr.sin_port));

    SOCKET client_sock = INVALID_SOCKET;
    struct sockaddr_in client_addr;
    int len = sizeof(client_addr);
    while (true) {
        if ((client_sock = accept(server.tcp_socket, (struct sockaddr *)&client_addr, &len)) == INVALID_SOCKET) {
            log_error("Error while accepting socket. %d", WSAGetLastError());
            continue;
        }
    }
}

DWORD WINAPI server_listen_udp(unused void *arg) {
    log_info("Listening on UDP port %d.", ntohs(server.udp_addr.sin_port));

    char *buffer = calloc(1, 1024);
    int len = SOCKET_ERROR;
    while (true) {
        if ((len = recv(server.udp_socket, buffer, 1024, 0)) == SOCKET_ERROR) {
            log_error("Error while recieving UDP Datagram.");
            continue;
        }

        if (buffer[0] != INTERMEDIATE_HEADER || buffer[len] != INTERMEDIATE_END) {
            log_error("Invalid packet.");
            continue;
        }

        result_t res;
        if ((res = intermediate_from_buffer(&server.events, buffer, len)).is_error) {
            log_error(res.description);
            result_discard(res);
            continue;
        }
    }
    return 0;
}
