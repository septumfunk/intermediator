#pragma once
#include "../util/win.h"
#include "../util/ext.h"
#include "../scripting/scripting_api.h"
#include "../structures/hashtable.h"

#define SERVER_DEFAULT_PORT 5060

typedef struct server_t {
    SOCKET tcp_socket, udp_socket;
    struct sockaddr_in tcp_addr, udp_addr;
    HANDLE tcp_thread, udp_thread;

    scripting_api_t api;
    hashtable_t clients, clients_addr;
    uint32_t max_players;
    intermediate_t *events;
    HANDLE event_mutex, clients_mutex;
} server_t;
extern server_t server;

void server_start(void);
void server_init_winsock(void);
void server_stop(void);

void server_process_events(void);

DWORD WINAPI server_listen_tcp(unused void *arg);
DWORD WINAPI server_listen_udp(unused void *arg);