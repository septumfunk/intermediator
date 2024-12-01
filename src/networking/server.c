#include "server.h"
#include "../win32/win32.h"
#include "../win32/socket.h"
#include "../util/log.h"
#include "../util/stringext.h"
#include "../scripting/scripting_api.h"
#include "client.h"
#include <math.h>
#include <minwinbase.h>
#include <processthreadsapi.h>
#include <stdlib.h>
#include <string.h>
#include <synchapi.h>
#include <winsock2.h>
#include <time.h>

server_t server;

void server_start(void) {
    srand(time(NULL));

    result_t res;
    if ((res = scripting_api_new(&server.api)).is_error) {
        log_error(res.description);
        result_discard(res);
        server_stop();
    }
    server.intermediate_buffer = NULL;

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
    if ((res = scripting_api_config_number(&server.api, "tcp_port", &port, SERVER_DEFAULT_PORT)).is_error) {
        log_error(res.description);
        result_discard(res);
        server_stop();
    }
    server.tcp_addr.sin_port = htons((u_short)floorf(port));

    if (bind(server.tcp_socket, (struct sockaddr *)&server.tcp_addr, sizeof(struct sockaddr)) == SOCKET_ERROR) {
        log_error("Failed to bind TCP socket.");
        server_stop();
    }
    server.tcp_thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)server_listen_tcp, NULL, 0, NULL);

    server.udp_socket = INVALID_SOCKET;
    if ((server.udp_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == INVALID_SOCKET) {
        log_error("Failed to open UDP socket.");
        server_stop();
    }
    log_info("Opened UDP socket.");
    server.udp_addr.sin_family = AF_INET;
    server.udp_addr.sin_addr.s_addr = inet_addr("0.0.0.0");

    if ((res = scripting_api_config_number(&server.api, "udp_port", &port, SERVER_DEFAULT_PORT)).is_error) {
        log_error(res.description);
        result_discard(res);
        server_stop();
    }
    server.udp_addr.sin_port = htons((u_short)floorf(port));

    if (bind(server.udp_socket, (struct sockaddr *)&server.udp_addr, sizeof(struct sockaddr)) == SOCKET_ERROR) {
        log_error("Failed to bind UDP socket.");
        server_stop();
    }
    server.udp_thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)server_listen_udp, NULL, 0, NULL);

    server.clients = hashtable_string();
    server.clients_addr = hashtable_string();

    float max_players;
    if ((res = scripting_api_config_number(&server.api, "max_players", &max_players, 512)).is_error) {
        log_error(res.description);
        result_discard(res);
        server_stop();
    }
    server.max_players = max_players;

    server.intermediate_mutex = mutex_new();

    server_process_events();
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

    hashtable_delete(&server.clients);
    hashtable_delete(&server.clients_addr);

    mutex_delete(server.intermediate_mutex);

    TerminateThread(server.tcp_thread, 0);
    TerminateThread(server.udp_thread, 0);

    exit(-1);
}

void server_process_events(void) {
    intermediate_t *inter;
    while (true) {
        inter = server_intermediate_pop();
        if (!inter)
            continue;

        result_t res;
        if ((res = scripting_api_try_event(&server.api, inter)).is_error) {
            if (result_match(res, "LuaEventErr")) {
                log_error(res.description);
            }
            result_discard(res);
        }
        intermediate_delete(inter);
    }
}

void server_intermediate_push(intermediate_t *intermediate, char *uuid) {
    mutex_lock(server.intermediate_mutex);
    if (intermediate->client_uuid)
        free(intermediate->client_uuid);
    intermediate->client_uuid = _strdup(uuid);

    if (!server.intermediate_buffer) {
        server.intermediate_buffer = intermediate;
        mutex_release(server.intermediate_mutex);
        return;
    }

    server.intermediate_buffer->previous = intermediate;
    intermediate->next = server.intermediate_buffer;
    server.intermediate_buffer = intermediate;
    mutex_release(server.intermediate_mutex);
}

intermediate_t *server_intermediate_pop(void) {
    mutex_lock(server.intermediate_mutex);
    intermediate_t *head = server.intermediate_buffer;
    while (head && head->next)
        head = head->next;
    if (!head) {
        mutex_release(server.intermediate_mutex);
        return NULL;
    }

    if (head == server.intermediate_buffer)
        server.intermediate_buffer = NULL;

    if (head->previous) head->previous->next = NULL;
    head->previous = NULL;
    head->next = NULL;
    mutex_release(server.intermediate_mutex);

    return head;
}

DWORD WINAPI server_listen_tcp(unused void *arg) {
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
        client_t *c = client_new(client_sock, client_addr);
        if (server.clients.pair_count > server.max_players)
            client_kick(c, "Server is full.");
    }

    return 0;
}

DWORD WINAPI server_listen_udp(unused void *arg) {
    log_info("Listening on UDP port %d.", ntohs(server.udp_addr.sin_port));

    char *buffer = calloc(1, MAX_INTERMEDIATE_SIZE);
    int len = SOCKET_ERROR;

    struct sockaddr_in addr;
    int a_len = sizeof(addr);
    while (true) {
        memset(buffer, 0, MAX_INTERMEDIATE_SIZE);
        if ((len = recvfrom(server.udp_socket, buffer, MAX_INTERMEDIATE_SIZE, 0, (struct sockaddr *)&addr, &a_len)) == SOCKET_ERROR) {
            log_error("Error while recieving UDP Datagram.");
            continue;
        }

        char *start = NULL;
        for (char *seek = buffer; seek < buffer + MAX_INTERMEDIATE_SIZE; ++seek) {
            if ((intermediate_control_e)*seek == INTERMEDIATE_HEADER) {
                start = seek;
                break;
            }
        }

        if (!start) {
            log_error("UDP Event data is unreadable");
            continue;
        }

        // Look up
        void *ptr;
        client_t *client;
        char *addrf = address_string(addr);
        if (!(ptr = hashtable_get(&server.clients_addr, addrf)) || !(client = *(client_t **)ptr))
            continue;
        free(addrf);

        if (!client)
            continue;
        char *uuid = client->uuid;

        result_t res;
        intermediate_t *intermediate = NULL;
        if ((res = intermediate_from_buffer(&intermediate, start, len, uuid)).is_error) {
            log_error(res.description);
            result_discard(res);
            continue;
        }

        if (intermediate)
            server_intermediate_push(intermediate, uuid);
    }
    return 0;
}
