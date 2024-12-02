#pragma once
#include "../data/mutex.h"
#include "../util/ext.h"
#include "../api/scripting_api.h"
#include "../data/hashtable.h"

#define SERVER_DEFAULT_PORT 5060

typedef struct server_t {
    uint32_t max_players;

    SOCKET tcp_socket, udp_socket;
    struct sockaddr_in tcp_addr, udp_addr;
    HANDLE udp_thread;

    scripting_api_t api;
    hashtable_t clients, clients_addr;
} server_t;
extern server_t server;

void server_start(void);
void server_init_tcp(void);
void server_init_udp(void);
void server_stop(void);

void server_listen_tcp(void);
DWORD WINAPI server_listen_udp(unused void *arg);