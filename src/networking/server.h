#pragma once
#include "../win32/mutex.h"
#include "../util/ext.h"
#include "../scripting/scripting_api.h"
#include "../structures/hashtable.h"

#define SERVER_DEFAULT_PORT 5060

typedef struct server_t {
    uint32_t max_players;

    SOCKET tcp_socket, udp_socket;
    struct sockaddr_in tcp_addr, udp_addr;
    HANDLE tcp_thread, udp_thread;

    scripting_api_t api;
    hashtable_t clients, clients_addr;

    intermediate_t *intermediate_buffer;
    mutex_t intermediate_mutex;
} server_t;
extern server_t server;

void server_start(void);
void server_init_winsock(void);
void server_stop(void);

void server_process_events(void);

void server_intermediate_push(intermediate_t *intermediate, char *uuid);
intermediate_t *server_intermediate_pop(void);

DWORD WINAPI server_listen_tcp(unused void *arg);
DWORD WINAPI server_listen_udp(unused void *arg);