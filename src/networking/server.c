#include "server.h"
#include "../util/win.h"
#include "../util/log.h"
#include "../util/stringext.h"
#include "../scripting/scripting_api.h"
#include "client.h"
#include <math.h>
#include <minwinbase.h>
#include <stdlib.h>
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
    server.tcp_thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)server_listen_tcp, NULL, 0, NULL);

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
    server.udp_thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)server_listen_udp, NULL, 0, NULL);

    server.event_mutex = CreateMutex(NULL, false, "Events_Mutex");
    server.clients_mutex = CreateMutex(NULL, false, "Clients_Mutex");

    server.clients = hashtable_string();
    server.clients_addr = hashtable_string();

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
    exit(-1);
}

void server_process_events(void) {
    while (true) {
        intermediate_t *inter = NULL;
        DWORD dwWait = WaitForSingleObject(server.event_mutex, INFINITE);
        if ((dwWait == 0) || (dwWait == WAIT_ABANDONED)) {
            inter = intermediates_pop(&server.events);
            ReleaseMutex(server.event_mutex);
        }

        if (!inter)
            continue;

        result_t res;
        if ((res = scripting_api_try_event(&server.api, inter)).is_error)
            result_discard(res);
        intermediate_delete(inter);
    }
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
        client_new(client_sock, client_addr);
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

        char *uuid;
        DWORD dwWait = WaitForSingleObject(server.clients_mutex, INFINITE);
        if ((dwWait == 0) || (dwWait == WAIT_ABANDONED)) {
            char *addrf = format("%s:%d", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
            client_t *client = *(client_t **)hashtable_get(&server.clients_addr, addrf);
            free(addrf);

            if (!client) {
                ReleaseMutex(server.event_mutex);
                continue;
            }
            uuid = client->uuid;
            ReleaseMutex(server.event_mutex);
        }

        result_t res;
        dwWait = WaitForSingleObject(server.event_mutex, INFINITE);
        if ((dwWait == 0) || (dwWait == WAIT_ABANDONED)) {
            if ((res = intermediate_from_buffer(&server.events, start, len, uuid)).is_error) {
                log_error(res.description);
                result_discard(res);
                continue;
            }
            ReleaseMutex(server.event_mutex);
        }
    }
    return 0;
}
