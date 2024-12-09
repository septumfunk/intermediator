#pragma once
#include "../util/win32.h"
#include "../util/ext.h"
#define CURL_STATICLIB 1
#include <curl/curl.h>
#include <stdint.h>

#define HTTP_DEFAULT_PORT 80
#define HTTP_REQUEST_MAX 1024

typedef struct http_server_t {
    SOCKET socket;
    HANDLE thread;
    CURL *curl;
    struct sockaddr_in address;

    bool accounts_enabled;
    char *discord_id, *discord_secret, *redirect_uri, *verify_url;
} http_server_t;
extern http_server_t http_server;

void http_server_init(void);
void http_server_cleanup(void);

unsigned long http_server_handle(unused void *arg);
char *http_server_process_request(struct sockaddr_in address, const char *uri);