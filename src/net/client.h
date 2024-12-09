#pragma once
#include "../data/mutex.h"
#include "../api/intermediate.h"
#include "discord.h"
#include <stdbool.h>
#include <stdint.h>
#include <winsock2.h>

#define UUID_LENGTH 16
#define UUID_CHARACTERS "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_"

typedef struct client_t {
    char *uuid;
    discord_id_t account;
    mutex_t mutex;

    SOCKET socket;
    struct sockaddr_in address;
    HANDLE thread;
} client_t;

client_t *client_new(SOCKET socket, struct sockaddr_in address);
void client_delete(client_t *self);

char *client_generate_uuid(void);
DWORD WINAPI client_handle(client_t *self);

void client_kick(client_t *self, const char *reason);
result_t client_send_intermediate(client_t *self, intermediate_t *intermediate);

void client_verify(client_t *self, discord_id_t account, const char *username);