#pragma once
#include "../util/win.h"
#include "../util/ext.h"
#include "../scripting/scripting_api.h"

typedef struct server_t {
    SOCKET tcp_socket, udp_socket;
    struct sockaddr_in tcp_addr, udp_addr;
    HANDLE tcp_thread, udp_thread;
    LPDWORD tcp_id, udp_id;

    scripting_api_t api;
    intermediate_t *events;
} server_t;
extern server_t server;

void server_start(void);
void server_init_winsock(void);
void server_stop(void);

void server_listen_tcp(void);
DWORD WINAPI server_listen_udp(unused void *arg);