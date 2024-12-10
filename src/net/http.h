#pragma once
#include "../io/fs.h"
#include "../util/win32.h"
#include "../util/ext.h"
#define CURL_STATICLIB 1
#include <curl/curl.h>
#include <stdint.h>

#define HTTP_DEFAULT_PORT 80
#define HTTP_REQUEST_MAX 1024

#define HTTP_DEFAULT_404 "<!DOCTYPE html><html><head></head><body><center><h1>404 Not Found!</h1><p>I couldn't find the resource \"%s\" for you, sorry!</p></center><style>html { background: rgb(56, 59, 67); font-family: 'Segoe UI', Tahoma, sans-serif; color: white; }</style></body></html>"
#define HTTP_DEFAULT_LOGIN_OK "<!DOCTYPE html><html><head></head><body><center><h1>You're logged in!</h1><p>Please return to the game.</p></center><style>html { background: rgb(56, 59, 67); font-family: 'Segoe UI', Tahoma, sans-serif; color: white; }</style></body></html>"
#define HTTP_DEFAULT_LOGIN_ERROR "<!DOCTYPE html><html><head></head><body><center><h1>Account Error</h1><p>%s</p></center><style>html { background: rgb(56, 59, 67); font-family: 'Segoe UI', Tahoma, sans-serif; color: white; } p { color: #FFBBBB; }</style></body></html>"

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
char *http_server_process_request(struct sockaddr_in address, const char *uri, fs_size_t *size);