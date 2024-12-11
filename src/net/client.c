#include "client.h"
#include "discord.h"
#include "server.h"
#include "socket.h"
#include "../api/intermediate.h"
#include "../data/stringext.h"
#include "../io/console.h"
#include <stdint.h>
#include <stdlib.h>
#include <winsock2.h>
#include <stdbool.h>

client_t *client_new(SOCKET socket, struct sockaddr_in address) {
    client_t *client = calloc(1, sizeof(client_t));
    *client = (client_t) {
        .uuid = client_generate_uuid(),
        .account = 0,
        .mutex = mutex_create(),

        .socket = socket,
        .address = address,
    };

    if (!client->uuid) {
        return nullptr;
    }

    // Insert into tables
    hashtable_insert(&server.clients, client->uuid, &client, sizeof(client_t *));
    char *addr = address_string(address);
    hashtable_insert(&server.clients_addr, addr, &client, sizeof(client_t *));
    free(addr);

    if (!server.login) {
        char *username = format("Player %s", client->uuid);
        client_verify(client, 1, username);
        free(username);
    } else {
        intermediate_t *intermediate = intermediate_new("verify", 0);
        intermediate_add_var(intermediate, "url", INTERMEDIATE_STRING, http_server.verify_url, strlen(http_server.verify_url) + 1);
        result_t res;
        if (!(res = client_send_intermediate(client, intermediate)).is_ok) {
            client_kick(client, "Unable to send URI.");
            return nullptr;
        }
    }

    client->thread = CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)client_handle, client, 0, nullptr);

    return client;
}

void client_delete(client_t *self) {
    mutex_lock(self->mutex);

    // Disconnect Event
    if (self->account) {
        result_t res;
        intermediate_t *intermediate = intermediate_new("disconnect", 0);
        if (!(res = scripting_api_try_event(&server.api, intermediate, self->uuid)).is_ok) {
            console_error(res.description);
            result_discard(res);
        }
        intermediate_delete(intermediate);

        scripting_api_delete_client(&server.api, self->uuid);
    }
    closesocket(self->socket);

    self->account = 0;

    // Remove from tables
    char *tofree = self->uuid;
    hashtable_remove(&server.clients, self->uuid);
    free(tofree);
    char *addr = address_string(self->address);
    hashtable_remove(&server.clients_addr, addr);
    free(addr);

    mutex_release(self->mutex);
    mutex_delete(self->mutex);
    TerminateThread(self->thread, 0);
}

char *client_generate_uuid(void) {
    int max = strlen(UUID_CHARACTERS);
    char *out = calloc(1, UUID_LENGTH + 1);

    unsigned char random_bytes[UUID_LENGTH];
    if (BCryptGenRandom(nullptr, random_bytes, UUID_LENGTH, BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0)
        return nullptr;

    for (int i = 0; i < UUID_LENGTH; ++i)
        out[i] = UUID_CHARACTERS[random_bytes[i] % max];

    return out;
}

DWORD WINAPI client_handle(client_t *self) {
    intermediate_control_e cc = INTERMEDIATE_NONE;
    uint64_t len = 0;
    char *buffer = calloc(1, MAX_INTERMEDIATE_SIZE);
    while (true) {
        if (recv(self->socket, buffer + len, sizeof(char), 0) <= 0) {
            mutex_release(self->mutex);
            client_delete(self);
        }

        if (!self->account)
            continue;

        switch (buffer[len]) {
            case INTERMEDIATE_HEADER: {
                mutex_lock(self->mutex);
                if (cc != INTERMEDIATE_NONE) {
                    cc = INTERMEDIATE_NONE;
                    continue;
                }

                cc = INTERMEDIATE_HEADER;
                len++;

                // Recieve version and id/reply
                if (recv(self->socket, buffer + len, sizeof(float) + sizeof(uint32_t) * 2, 0)  <= 0) {
                    mutex_release(self->mutex);
                    client_delete(self);
                    return 0;
                }
                len += sizeof(float) + sizeof(uint32_t) * 2;

                int olen = len;
                while (true) {
                    if (recv(self->socket, buffer + len, sizeof(char), 0) == SOCKET_ERROR) {
                        mutex_release(self->mutex);
                        client_delete(self);                        return 0;
                    }
                    if (buffer[len] == '\0' && olen - len >= MAX_INTERMEDIATE_STRING_LENGTH - 1) {
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
                    if (recv(self->socket, buffer + len, sizeof(char), 0) == SOCKET_ERROR) {
                        mutex_release(self->mutex);
                        client_delete(self);
                        return 0;
                    }
                    if (buffer[len] == '\0' && olen - len >= MAX_INTERMEDIATE_STRING_LENGTH - 1) {
                        len++;
                        break;
                    }
                    len++;
                }

                // Type
                if (recv(self->socket, buffer + len, sizeof(char), 0) == SOCKET_ERROR) {
                    mutex_release(self->mutex);
                    client_delete(self);
                    return 0;
                }
                len++;

                // Value
                switch ((intermediate_type_e)*(buffer + len - 1)) {
                    case INTERMEDIATE_STRING:
                        olen = len;
                        while (true) {
                            if (recv(self->socket, buffer + len, sizeof(char), 0) == SOCKET_ERROR) {
                                mutex_release(self->mutex);
                                client_delete(self);
                                return 0;
                            }
                            if (buffer[len] == '\0' && olen - len >= MAX_INTERMEDIATE_STRING_LENGTH - 1) {
                                len++;
                                break;
                            }
                            len++;
                        }
                        break;

                    case INTERMEDIATE_S8:
                    case INTERMEDIATE_U8:
                        if (recv(self->socket, buffer + len, sizeof(int8_t), 0) == SOCKET_ERROR) {
                            mutex_release(self->mutex);
                            client_delete(self);
                            return 0;
                        }
                        len += sizeof(int8_t);
                        break;

                    case INTERMEDIATE_S16:
                    case INTERMEDIATE_U16:
                        if (recv(self->socket, buffer + len, sizeof(int16_t), 0) == SOCKET_ERROR) {
                            mutex_release(self->mutex);
                            client_delete(self);
                            return 0;
                        }
                        len += sizeof(int16_t);
                        break;

                    case INTERMEDIATE_S32:
                    case INTERMEDIATE_U32:
                    case INTERMEDIATE_F32:
                        if (recv(self->socket, buffer + len, sizeof(int32_t), 0) == SOCKET_ERROR) {
                            mutex_release(self->mutex);
                            client_delete(self);
                            return 0;
                        }
                        len += sizeof(int32_t);
                        break;

                    case INTERMEDIATE_S64:
                    case INTERMEDIATE_U64:
                    case INTERMEDIATE_F64:
                        if (recv(self->socket, buffer + len, sizeof(int64_t), 0) == SOCKET_ERROR) {
                            mutex_release(self->mutex);
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

                result_t res;
                intermediate_t *intermediate = nullptr;
                if (!(res = intermediate_deserialize(buffer, &intermediate)).is_ok) {
                    console_error(res.description);
                    result_discard(res);

                    memset(buffer, 0, MAX_INTERMEDIATE_SIZE);
                    cc = INTERMEDIATE_NONE;
                    len = 0;
                    mutex_release(self->mutex);
                    Sleep(33);
                    continue;
                }

                // Process
                if (!(res = scripting_api_try_event(&server.api, intermediate, self->uuid)).is_ok) {
                    console_error(res.description);
                    result_discard(res);
                }
                intermediate_delete(intermediate);

                memset(buffer, 0, MAX_INTERMEDIATE_SIZE);
                cc = INTERMEDIATE_NONE;
                len = 0;

                mutex_release(self->mutex);
                Sleep(33);
                continue;
            }
            default: continue;
        }

        mutex_release(self->mutex);
    }

    return 0;
}

void client_kick(client_t *self, const char *reason) {
    console_log("Client '%s' was kicked for: '%s'", self->uuid, reason);

    // Goodbye
    intermediate_t *intermediate = intermediate_new("kick", 0);
    intermediate_add_var(intermediate, "reason", INTERMEDIATE_STRING, (void *)reason, strlen(reason) + 1);

    int len = 0;
    char *buffer = intermediate_serialize(intermediate, &len);
    free(intermediate);

    send(self->socket, buffer, len, 0);
    free(buffer);
    Sleep(100);
    client_delete(self);
}

void client_verify(client_t *self, discord_id_t account, const char *username) {
    if (account && !self->account) {
        // Create Client
        scripting_api_create_client(&server.api, self->uuid, self->address, account, username);

        // Connect Event
        result_t res;
        intermediate_t *intermediate = intermediate_new("connect", 0);
        if (!(res = scripting_api_try_event(&server.api, intermediate, self->uuid)).is_ok) {
            console_error(res.description);
            result_discard(res);
        }
        intermediate_delete(intermediate);

        self->account = account;
    }
}

result_t client_send_intermediate(client_t *self, intermediate_t *intermediate) {
    int len = 0;
    char *buffer = intermediate_serialize(intermediate, &len);

    mutex_lock(self->mutex);
    if (send(self->socket, buffer, len, 0) == SOCKET_ERROR) {
        mutex_release(self->mutex);
        return result_error("Failed to send intermediate '%s'.", intermediate->type);
    }
    mutex_release(self->mutex);

    return result_ok();
}