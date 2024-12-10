#include "http.h"
#include "client.h"
#include "discord.h"
#include "server.h"
#include "socket.h"
#include "../io/console.h"
#include "../io/fs.h"
#include "../data/stringext.h"
#include <curl/curl.h>
#include <curl/easy.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>

http_server_t http_server;

void http_server_init(void) {
    console_header("Initializing HTTP Server");
    http_server.socket = INVALID_SOCKET;
    if ((http_server.socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == INVALID_SOCKET) {
        winsock_console_error();
        server_stop();
    }
    console_log("Opened HTTP socket.");
    http_server.address.sin_family = AF_INET;
    http_server.address.sin_addr.s_addr = inet_addr("0.0.0.0");

    float port;
    result_discard(scripting_api_config_number(&server.api, "http_port", &port, HTTP_DEFAULT_PORT));
    http_server.address.sin_port = htons((u_short)floorf(port));

    if (bind(http_server.socket, (struct sockaddr *)&http_server.address, sizeof(struct sockaddr)) == SOCKET_ERROR) {
        winsock_console_error();
        server_stop();
    }
    console_log("Bound HTTP socket to port %d.", (int)floorf(port));

    if (listen(http_server.socket, 6) == SOCKET_ERROR) {
        winsock_console_error();
        server_stop();
    }
    console_log("Listening on HTTP port %d.", ntohs(http_server.address.sin_port));

    if (!fs_direxists("http"))
        fs_mkdir("http");
    if (!fs_direxists("http/verify"))
        fs_mkdir("http/verify");

    result_t res;
    if (!fs_exists("http/verify/ok.html") && !(res = fs_save("http/verify/ok.html", HTTP_DEFAULT_LOGIN_OK, strlen(HTTP_DEFAULT_LOGIN_OK))).is_ok) {
        console_error(res.description);
        result_discard(res);
        server_stop();
    }

    if (!fs_exists("http/verify/error.html") && !(res = fs_save("http/verify/error.html", HTTP_DEFAULT_LOGIN_ERROR, strlen(HTTP_DEFAULT_LOGIN_ERROR))).is_ok) {
        console_error(res.description);
        result_discard(res);
        server_stop();
    }

    http_server.thread = CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)http_server_handle, nullptr, 0, nullptr);

    float accounts_enabled;
    if ((res = scripting_api_config_number(&server.api, "accounts_enabled", &accounts_enabled, true)).is_ok && (http_server.accounts_enabled = accounts_enabled)) {
        if (!(res = scripting_api_config_string(&server.api, "discord_id", &http_server.discord_id, nullptr)).is_ok || !http_server.discord_id) {
            result_discard(res);
            console_error("Unable to find Discord ID. Add net.config.discord_id (string) to config.lua or disable net.config.accounts_enabled to continue.");
            server_stop();
        }

        if (!(res = scripting_api_config_string(&server.api, "discord_secret", &http_server.discord_secret, nullptr)).is_ok || !http_server.discord_secret) {
            result_discard(res);
            console_error("Unable to find Discord Secret. Add net.config.discord_secret (string) to config.lua or disable net.config.accounts_enabled to continue.");
            server_stop();
        }

        if (!(res = scripting_api_config_string(&server.api, "redirect_uri", &http_server.redirect_uri, nullptr)).is_ok || !http_server.redirect_uri) {
            result_discard(res);
            console_error("Unable to find Redirect URI. Add net.config.redirect_uri (string) to config.lua or disable net.config.accounts_enabled to continue.");
            server_stop();
        }

        if (!(res = scripting_api_config_string(&server.api, "verify_url", &http_server.verify_url, nullptr)).is_ok || !http_server.verify_url) {
            result_discard(res);
            console_error("Unable to find Verify URL. Add net.config.verify_url (string) to config.lua or disable net.config.accounts_enabled to continue.");
            server_stop();
        }
    }

    // Init curl
    curl_global_init(CURL_GLOBAL_ALL);
    http_server.curl = curl_easy_init();
}

void http_server_cleanup(void) {
    closesocket(http_server.socket);
    curl_easy_cleanup(http_server.curl);
    curl_global_cleanup();
}

unsigned long http_server_handle(unused void *arg) {
    SOCKET request_socket = INVALID_SOCKET;
    struct sockaddr_in request_addr;
    int len = sizeof(request_addr);

    uint32_t buffer_size;
    char *buffer = calloc(HTTP_REQUEST_MAX, sizeof(char));

    char method[10];
    char uri[100];
    char version[10];

    while (true) {
        if ((request_socket = accept(http_server.socket, (struct sockaddr *)&request_addr, &len)) == INVALID_SOCKET) {
            winsock_console_error();
            continue;
        }

        if ((buffer_size = recv(request_socket, buffer, HTTP_REQUEST_MAX, 0)) <= 0 || sscanf(buffer, "%s %s %s", method, uri, version) != 3)
            goto cleanup;

        fs_size_t size;
        char *res = http_server_process_request(request_addr, uri, &size);
        char *touse = res;

        char *error404 = nullptr;
        if (!res) {
            result_t mres;
            if (!(mres = fs_load("http/404.html", &error404, &size)).is_ok || !error404) {
                result_discard(mres);
                touse = format(HTTP_DEFAULT_404, uri);
                size = strlen(touse);
            } else touse = format(error404, uri);
            free(error404);
        }

        if (!size)
            size = strlen(touse);

        char *content_type = "text/html";
        if (strstr(uri, "css"))
            content_type = "text/css";
        if (strstr(uri, "png"))
            content_type = "image/png";

        char *fmt = format(
            "HTTP/1.1 %s\r\n"
            "Content-Length: %d\r\n"
            "Content-Type: %s\r\n"
            "\r\n",
            res ? "200 OK" : "404 Not Found",
            size,
            content_type
        );

        char *full_response = calloc(1, strlen(fmt) + size);
        memcpy(full_response, fmt, strlen(fmt));
        memcpy(full_response + strlen(fmt), touse, size);

        send(request_socket, full_response, strlen(fmt) + size, 0);
        free(full_response);
        free(fmt);
        if (res) free(res);
        else free(touse);

    cleanup:
        Sleep(10);
        closesocket(request_socket);
        memset(buffer, 0, HTTP_REQUEST_MAX);
        len = sizeof(request_addr);
    }
    return 0;
}

char *http_server_process_request(struct sockaddr_in address, const char *uri, fs_size_t *size) {
    int len = strlen(uri) + 1;
    char path[len], query[len];
    char varname[len], value[len];
    if (sscanf(uri, "%[^?]?%s", path, query) == 2) { // Has parameter
        if (sscanf(query, "%[^=]=%s", varname, value) != 2)
            return nullptr;
    }

    if (bstrcmp("/verify", path)) {
        if (bstrcmp(varname, "code")) {
            uint32_t count = 0;

            result_t res;
            discord_id_t account = 0;
            const char *username = nullptr;
            if (!(res = discord_info_from_code(value, &account, &username)).is_ok || !account || !username) {
                char *error = nullptr;
                result_t mres;
                if (!(mres = fs_load("http/verify/error.html", &error, size)).is_ok || !error) {
                    result_discard(mres);

                    char *ret = format(HTTP_DEFAULT_LOGIN_ERROR, res.description);
                    *size = strlen(ret) + 1;
                    return ret;
                }

                console_log(res.description);
                char *errordoc = format(error, res.description);
                *size = strlen(errordoc) + 1;
                free(error);
                result_discard(res);

                return errordoc;
            }

            pair_t **pairs = hashtable_pairs(&server.clients, &count);
            mutex_lock(server.clients.mutex);
            for (pair_t **pl = pairs; pl < pairs + count; ++pl) {
                client_t *client = *(client_t **)(*pl)->value;
                if (!client->account && client->address.sin_addr.S_un.S_addr == address.sin_addr.S_un.S_addr) {
                    mutex_lock(client->mutex);
                    client_verify(client, account, username);
                    mutex_release(client->mutex);
                }
            }
            mutex_release(server.clients.mutex);

            char *ok = nullptr;
            if (!(res = fs_load("http/verify/ok.html", &ok, size)).is_ok || !ok) {
                result_discard(res);
                char *ret = _strdup(HTTP_DEFAULT_LOGIN_OK);
                *size = strlen(ret) + 1;
                return ret;
            }

            return ok;
        }
    }

    result_t res;
    char *html;
    char *p = format("http%s", uri);
    if (!(res = fs_load(p, &html, size)).is_ok) {
        free(p);
        result_discard(res);
        return nullptr;
    }
    free(p);

    return html;
}