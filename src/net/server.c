#include "server.h"
#include "socket.h"
#include "../util/win32.h"
#include "../io/console.h"
#include "../data/stringext.h"
#include "../api/scripting_api.h"
#include "client.h"
#include <math.h>
#include <minwinbase.h>
#include <processthreadsapi.h>
#include <stdlib.h>
#include <string.h>

server_t server;

void server_start(void) {
    // Initialize Dependencies
    console_init();
    winsock_init();

    result_t res = scripting_api_new(&server.api);
    if (!res.is_ok) {
        console_error(res.description);
        result_discard(res);
        server_stop();
    }

    // Initialize Server
    server.clients = hashtable_string();
    server.clients_addr = hashtable_string();

    float max_players;
    if (!(res = scripting_api_config_number(&server.api, "max_players", &max_players, 512)).is_ok) {
        console_error(res.description);
        result_discard(res);
        server_stop();
    }
    server.max_players = max_players;

    server_init_tcp();
    server_init_udp();

    console_header("Starting Server");
    server_listen_tcp();
}

void server_init_tcp(void) {
    server.tcp_socket = INVALID_SOCKET;
    if ((server.tcp_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == INVALID_SOCKET) {
        winsock_console_error();
        server_stop();
    }
    console_log("Opened TCP socket.");
    server.tcp_addr.sin_family = AF_INET;
    server.tcp_addr.sin_addr.s_addr = inet_addr("0.0.0.0");

    result_t res;
    float port;
    if (!(res = scripting_api_config_number(&server.api, "tcp_port", &port, SERVER_DEFAULT_PORT)).is_ok) {
        winsock_console_error();
        result_discard(res);
        server_stop();
    }
    server.tcp_addr.sin_port = htons((u_short)floorf(port));

    if (bind(server.tcp_socket, (struct sockaddr *)&server.tcp_addr, sizeof(struct sockaddr)) == SOCKET_ERROR) {
        winsock_console_error();
        server_stop();
    }
}

void server_init_udp(void) {
    server.udp_socket = INVALID_SOCKET;
    if ((server.udp_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == INVALID_SOCKET) {
        winsock_console_error();
        server_stop();
    }
    console_log("Opened UDP socket.");
    server.udp_addr.sin_family = AF_INET;
    server.udp_addr.sin_addr.s_addr = inet_addr("0.0.0.0");

    result_t res;
    float port;
    if (!(res = scripting_api_config_number(&server.api, "udp_port", &port, SERVER_DEFAULT_PORT)).is_ok) {
        winsock_console_error();
        result_discard(res);
        server_stop();
    }
    server.udp_addr.sin_port = htons((u_short)floorf(port));

    if (bind(server.udp_socket, (struct sockaddr *)&server.udp_addr, sizeof(struct sockaddr)) == SOCKET_ERROR) {
        winsock_console_error();
        server_stop();
    }
    server.udp_thread = CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)server_listen_udp, nullptr, 0, nullptr);
}

void server_stop(void) {
    WSACleanup();

    hashtable_delete(&server.clients);
    hashtable_delete(&server.clients_addr);

    exit(EXIT_FAILURE);
}

void server_listen_tcp(void) {
    if (listen(server.tcp_socket, 6) == SOCKET_ERROR) {
        winsock_console_error();
        server_stop();
    }
    console_log("Listening on TCP port %d.", ntohs(server.tcp_addr.sin_port));

    SOCKET client_sock = INVALID_SOCKET;
    struct sockaddr_in client_addr;
    int len = sizeof(client_addr);
    while (true) {
        if ((client_sock = accept(server.tcp_socket, (struct sockaddr *)&client_addr, &len)) == INVALID_SOCKET) {
            winsock_console_error();
            continue;
        }

        client_t *c = client_new(client_sock, client_addr);
        if (!c) {
            free(c);
            continue;
        }

        if (server.clients.pair_count > server.max_players)
            client_kick(c, "Server is full.");
    }
}

DWORD WINAPI server_listen_udp(unused void *arg) {
    console_log("Listening on UDP port %d.", ntohs(server.udp_addr.sin_port));

    char *buffer = calloc(1, MAX_INTERMEDIATE_SIZE);
    int len = SOCKET_ERROR;

    struct sockaddr_in addr;
    int a_len = sizeof(addr);
    while (true) {
        memset(buffer, 0, MAX_INTERMEDIATE_SIZE);
        if ((len = recvfrom(server.udp_socket, buffer, MAX_INTERMEDIATE_SIZE, 0, (struct sockaddr *)&addr, &a_len)) == SOCKET_ERROR) {
            winsock_console_error();
            continue;
        }

        // Look up client
        void *ptr;
        client_t *client;
        char *addrf = address_string(addr);
        if (!(ptr = hashtable_get(&server.clients_addr, addrf)) || !(client = *(client_t **)ptr))
            continue;
        free(addrf);

        if (!client)
            continue;

        result_t res;
        intermediate_t *intermediate = nullptr;
        if (!(res = intermediate_from_buffer(buffer, len, &intermediate)).is_ok) {
            console_error(res.description);
            result_discard(res);
            continue;
        }

        if (intermediate) {
            if (!(res = scripting_api_try_event(&server.api, intermediate, client->uuid)).is_ok) {
                console_error(res.description);
                result_discard(res);
            }
        }
        intermediate_delete(intermediate);
    }
    return 0;
}
