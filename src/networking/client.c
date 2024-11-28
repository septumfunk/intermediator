#include "client.h"
#include "server.h"
#include "../scripting/intermediate.h"
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
        .socket = socket,
        .address = address,
    };

    DWORD dwWait = WaitForSingleObject(server.clients_mutex, INFINITE);
    if ((dwWait == 0) || (dwWait == WAIT_ABANDONED)) {
        hashtable_insert(&server.clients, client->uuid, &client, sizeof(client_t *));

        char *addr = format("%s:%d", inet_ntoa(client->address.sin_addr), ntohs(client->address.sin_port));
        hashtable_insert(&server.clients_addr, addr, &client, sizeof(client_t *));
        free(addr);

        scripting_api_create_client(&server.api, client->uuid, client->address);
        ReleaseMutex(server.clients_mutex);
    }
    client->thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)client_handle, client, 0, NULL);

    log_info("Client '%s' connected.", client->uuid);

    return client;
}

void client_delete(client_t *self) {
    log_info("Client '%s' disconnected.", self->uuid);
    TerminateThread(self->thread, 0);

    DWORD dwWait = WaitForSingleObject(server.clients_mutex, INFINITE);
    if ((dwWait == 0) || (dwWait == WAIT_ABANDONED)) {
        scripting_api_delete_client(&server.api, self->uuid);

        closesocket(self->socket);

        char *tofree = self->uuid;
        hashtable_remove(&server.clients, self->uuid);
        free(tofree);

        char *addr = format("%s:%d", inet_ntoa(self->address.sin_addr), ntohs(self->address.sin_port));
        hashtable_remove(&server.clients_addr, addr);
        free(addr);

        ReleaseMutex(server.clients_mutex);
    }
}

char *client_generate_uuid(void) {
    int max = strlen(UUID_CHARACTERS);
    char *out = calloc(1, UUID_LENGTH + 1);
    for (int i = 0; i < UUID_LENGTH; ++i)
        out[i] = UUID_CHARACTERS[rand() % (max + 1)];
    return out;
}

DWORD WINAPI client_handle(client_t *self) {
    intermediate_control_e cc = INTERMEDIATE_NONE;
    uint64_t len = 0;
    char *intermediate = calloc(1, MAX_INTERMEDIATE_SIZE);
    while (recv(self->socket, intermediate + len, sizeof(char), 0) > 0) {
        switch (intermediate[len]) {
            case INTERMEDIATE_HEADER: {
                if (cc != INTERMEDIATE_NONE) {
                    cc = INTERMEDIATE_NONE;
                    continue;
                }

                cc = INTERMEDIATE_HEADER;
                len++;

                // Recieve version and id/reply
                if (recv(self->socket, intermediate + len, sizeof(float) + sizeof(uint32_t) * 2, 0)  <= 0) {
                    client_delete(self);
                    return 0;
                }
                len += sizeof(float) + sizeof(uint32_t) * 2;

                int olen = len;
                while (true) {
                    if (recv(self->socket, intermediate + len, sizeof(char), 0) == SOCKET_ERROR) {
                        client_delete(self);
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
                            return 0;
                        }
                        len += sizeof(int8_t);
                        break;

                    case INTERMEDIATE_S16:
                    case INTERMEDIATE_U16:
                        if (recv(self->socket, intermediate + len, sizeof(int16_t), 0) == SOCKET_ERROR) {
                            client_delete(self);
                            return 0;
                        }
                        len += sizeof(int16_t);
                        break;

                    case INTERMEDIATE_S32:
                    case INTERMEDIATE_U32:
                    case INTERMEDIATE_F32:
                        if (recv(self->socket, intermediate + len, sizeof(int32_t), 0) == SOCKET_ERROR) {
                            client_delete(self);
                            return 0;
                        }
                        len += sizeof(int32_t);
                        break;

                    case INTERMEDIATE_S64:
                    case INTERMEDIATE_U64:
                    case INTERMEDIATE_F64:
                        if (recv(self->socket, intermediate + len, sizeof(int64_t), 0) == SOCKET_ERROR) {
                            client_delete(self);
                            return 0;
                        }
                        len += sizeof(int64_t);
                        break;
                }
                break;
            }
            case INTERMEDIATE_END: {
                len++;
                DWORD dwWait = WaitForSingleObject(server.event_mutex, INFINITE);
                if ((dwWait == 0) || (dwWait == WAIT_ABANDONED)) {
                    result_t res;
                    if ((res = intermediate_from_buffer(&server.events, intermediate, len, self->uuid)).is_error) {
                        log_error(res.description);
                        result_discard(res);
                    }
                    ReleaseMutex(server.event_mutex);
                }

                memset(intermediate, 0, MAX_INTERMEDIATE_SIZE);
                cc = INTERMEDIATE_NONE;
                len = 0;
            }
            default: continue;
        }
    }
    client_delete(self);
    return 0;
}

void client_kick(client_t *client, const char *reason) {
    intermediate_t *inter = intermediate_new("kick", 0);
}