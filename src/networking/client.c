#include "client.h"
#include "server.h"
#include "../scripting/intermediate.h"
#include "../win32/socket.h"
#include "../util/stringext.h"
#include <processthreadsapi.h>
#include <stdint.h>
#include <stdlib.h>
#include <winsock2.h>
#include <stdbool.h>

client_t *client_new(SOCKET socket, struct sockaddr_in address) {
    client_t *client = calloc(1, sizeof(client_t));
    *client = (client_t) {
        .uuid = client_generate_uuid(),
        .active = true,
        .mutex = mutex_new(),

        .socket = socket,
        .address = address,
    };

    // Insert into tables
    hashtable_insert(&server.clients, client->uuid, &client, sizeof(client_t *));
    char *addr = address_string(address);
    hashtable_insert(&server.clients_addr, addr, &client, sizeof(client_t *));
    free(addr);

    // Create Client
    scripting_api_create_client(&server.api, client->uuid, client->address);
    client->thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)client_handle, client, 0, NULL);

    // Welcome
    log_info("Client '%s' connected.", client->uuid);
    intermediate_t *inter = intermediate_new("welcome", 0);
    server_intermediate_push(inter, client->uuid);

    return client;
}

void client_delete(client_t *self) {
    mutex_lock(self->mutex);

    log_info("Client '%s' disconnected.", self->uuid);
    self->active = false;
    TerminateThread(self->thread, 0);

    scripting_api_delete_client(&server.api, self->uuid);
    closesocket(self->socket);

    // Remove from tables
    char *tofree = self->uuid;
    hashtable_remove(&server.clients, self->uuid);
    free(tofree);
    char *addr = address_string(self->address);
    hashtable_remove(&server.clients_addr, addr);
    free(addr);

    mutex_delete(self->mutex);
}

char *client_generate_uuid(void) {
    int max = strlen(UUID_CHARACTERS);
    char *out = calloc(1, UUID_LENGTH + 1);
    for (int i = 0; i < UUID_LENGTH; ++i)
        out[i] = UUID_CHARACTERS[rand() % (max + 1) - 1];
    return out;
}

DWORD WINAPI client_handle(client_t *self) {
    intermediate_control_e cc = INTERMEDIATE_NONE;
    uint64_t len = 0;
    char *intermediate = calloc(1, MAX_INTERMEDIATE_SIZE);
    while (self->active) {
        if (recv(self->socket, intermediate + len, sizeof(char), 0) <= 0)
            client_delete(self);

        switch (intermediate[len]) {
            case INTERMEDIATE_HEADER: {
                mutex_lock(self->mutex);
                if (cc != INTERMEDIATE_NONE) {
                    cc = INTERMEDIATE_NONE;
                    continue;
                }

                cc = INTERMEDIATE_HEADER;
                len++;

                // Recieve version and id/reply
                if (recv(self->socket, intermediate + len, sizeof(float) + sizeof(uint32_t) * 2, 0)  <= 0) {
                    client_delete(self);
                    mutex_release(self->mutex);
                    return 0;
                }
                len += sizeof(float) + sizeof(uint32_t) * 2;

                int olen = len;
                while (true) {
                    if (recv(self->socket, intermediate + len, sizeof(char), 0) == SOCKET_ERROR) {
                        client_delete(self);
                        mutex_release(self->mutex);
                        return 0;
                    }
                    if (intermediate[len] == '\0' && olen - len >= MAX_INTERMEDIATE_STRING_LENGTH - 1) {
                        len++;
                        break;
                    }
                    len++;
                }
                break;
            }
            case INTERMEDIATE_VARIABLE: {
                if (cc != INTERMEDIATE_HEADER && cc != INTERMEDIATE_VARIABLE) {
                    cc = INTERMEDIATE_NONE;
                    continue;
                }

                cc = INTERMEDIATE_VARIABLE;
                len++;

                // Name
                int olen = len;
                while (true) {
                    if (recv(self->socket, intermediate + len, sizeof(char), 0) == SOCKET_ERROR) {
                        client_delete(self);
                        mutex_release(self->mutex);
                        return 0;
                    }
                    if (intermediate[len] == '\0' && olen - len >= MAX_INTERMEDIATE_STRING_LENGTH - 1) {
                        len++;
                        break;
                    }
                    len++;
                }

                // Type
                if (recv(self->socket, intermediate + len, sizeof(char), 0) == SOCKET_ERROR) {
                    client_delete(self);
                    mutex_release(self->mutex);
                    return 0;
                }
                len++;

                // Value
                switch ((intermediate_type_e)*(intermediate + len - 1)) {
                    case INTERMEDIATE_STRING:
                        olen = len;
                        while (true) {
                            if (recv(self->socket, intermediate + len, sizeof(char), 0) == SOCKET_ERROR) {
                                client_delete(self);
                                mutex_release(self->mutex);
                                return 0;
                            }
                            if (intermediate[len] == '\0' && olen - len >= MAX_INTERMEDIATE_STRING_LENGTH - 1) {
                                len++;
                                break;
                            }
                            len++;
                        }
                        break;

                    case INTERMEDIATE_S8:
                    case INTERMEDIATE_U8:
                        if (recv(self->socket, intermediate + len, sizeof(int8_t), 0) == SOCKET_ERROR) {
                            client_delete(self);
                            mutex_release(self->mutex);
                            return 0;
                        }
                        len += sizeof(int8_t);
                        break;

                    case INTERMEDIATE_S16:
                    case INTERMEDIATE_U16:
                        if (recv(self->socket, intermediate + len, sizeof(int16_t), 0) == SOCKET_ERROR) {
                            client_delete(self);
                            mutex_release(self->mutex);
                            return 0;
                        }
                        len += sizeof(int16_t);
                        break;

                    case INTERMEDIATE_S32:
                    case INTERMEDIATE_U32:
                    case INTERMEDIATE_F32:
                        if (recv(self->socket, intermediate + len, sizeof(int32_t), 0) == SOCKET_ERROR) {
                            client_delete(self);
                            mutex_release(self->mutex);
                            return 0;
                        }
                        len += sizeof(int32_t);
                        break;

                    case INTERMEDIATE_S64:
                    case INTERMEDIATE_U64:
                    case INTERMEDIATE_F64:
                        if (recv(self->socket, intermediate + len, sizeof(int64_t), 0) == SOCKET_ERROR) {
                            client_delete(self);
                            mutex_release(self->mutex);
                            return 0;
                        }
                        len += sizeof(int64_t);
                        break;
                }
                break;
            }
            case INTERMEDIATE_END: {
                len++;

                 result_t res;
                intermediate_t *inter;
                if ((res = intermediate_from_buffer(&inter, intermediate, len, self->uuid)).is_error) {
                    log_error(res.description);
                    result_discard(res);
                }
                server_intermediate_push(inter, self->uuid);

                memset(intermediate, 0, MAX_INTERMEDIATE_SIZE);
                cc = INTERMEDIATE_NONE;
                len = 0;

                mutex_release(self->mutex);
                Sleep(33);
                break;
            }
            default: continue;
        }
    }

    return 0;
}

void client_kick(client_t *self, const char *reason) {
    log_info("Client '%s' was kicked for: '%s'", self->uuid, reason);

    // Goodbye
    intermediate_t *intermediate = intermediate_new("kick", 0);
    intermediate_add_var(intermediate, "reason", INTERMEDIATE_STRING, (void *)reason, strlen(reason) + 1);

    int len = 0;
    char *buffer = intermediate_to_buffer(intermediate, &len);
    free(intermediate);

    send(self->socket, buffer, len, 0);
    free(buffer);
    Sleep(100);
    client_delete(self);
}