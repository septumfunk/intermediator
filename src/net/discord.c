#include "discord.h"
#include "http.h"
#include "../data/stringext.h"
#include "../io/console.h"
#include <curl/easy.h>
#include <json-c/json.h>
#include <curl/curl.h>
#include <json_object.h>
#include <json_tokener.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct id_buffer_t { char *memory; int size; } id_buffer_t;

size_t _discord_id_write_callback(char *ptr, unused size_t size, size_t nmemb, void *userdata) {
    id_buffer_t *buf = userdata;
    buf->size += nmemb;
    buf->memory = realloc(buf->memory, buf->size);
    memcpy(buf->memory + buf->size - nmemb - 1, ptr, nmemb);
    buf->memory[buf->size - 1] = 0;
    return nmemb;
}

result_t discord_info_from_code(const char *code, discord_id_t *out, const char **out2) {
    curl_easy_setopt(http_server.curl, CURLOPT_URL, DISCORD_API_ENDPOINT "/oauth2/token");

    char *e_id = curl_easy_escape(http_server.curl, http_server.discord_id, strlen(http_server.discord_id));
    char *e_secret = curl_easy_escape(http_server.curl, http_server.discord_secret, strlen(http_server.discord_secret));
    char *e_code = curl_easy_escape(http_server.curl, code, strlen(code));
    char *e_uri = curl_easy_escape(http_server.curl, http_server.redirect_uri, strlen(http_server.redirect_uri));

    struct curl_slist *hs = NULL;
    hs = curl_slist_append(hs, "Content-Type: application/x-www-form-urlencoded");
    curl_easy_setopt(http_server.curl, CURLOPT_HTTPHEADER, hs);

    char *post = format("client_id=%s&client_secret=%s&grant_type=authorization_code&code=%s&redirect_uri=%s", e_id, e_secret, e_code, e_uri);
    curl_easy_setopt(http_server.curl, CURLOPT_POSTFIELDS, post);
    curl_easy_setopt(http_server.curl, CURLOPT_POSTFIELDSIZE, strlen(post));

    struct id_buffer_t {
        char *memory;
        int size;
    } data = { nullptr, 1 };
    curl_easy_setopt(http_server.curl, CURLOPT_WRITEFUNCTION, _discord_id_write_callback);
    curl_easy_setopt(http_server.curl, CURLOPT_WRITEDATA, (void *)&data);

    CURLcode result = curl_easy_perform(http_server.curl);
    free(post);
    free(e_uri);
    free(e_code);
    free(e_secret);
    free(e_id);

    if (result != CURLE_OK) {
        curl_easy_reset(http_server.curl);
        return result_error("Curl Error: %d", result);
    } else {
        struct json_object *obj = json_tokener_parse(data.memory);

        struct json_object *err = json_object_object_get(obj, "error");
        if (err) {
            const char *str = json_object_to_json_string_ext(obj, JSON_C_TO_STRING_PRETTY );
            curl_easy_reset(http_server.curl);
            return result_error("Discord API: %s", str);
        }

        struct json_object *token = json_object_object_get(obj, "access_token");
        const char *tokenstr;
        if (!token || !(tokenstr = json_object_get_string(token))) {
            curl_easy_reset(http_server.curl);
            return result_error("Discord API: Unable to find token in response: %s", json_object_to_json_string_ext(obj, JSON_C_TO_STRING_PRETTY ));
        }

        curl_easy_reset(http_server.curl);
        free(data.memory);
        data.memory = nullptr;
        data.size = 1;

        curl_easy_setopt(http_server.curl, CURLOPT_URL, DISCORD_API_ENDPOINT "/users/@me");

        char *header = format("Authorization: Bearer %s", tokenstr);
        hs = curl_slist_append(hs, header);
        curl_easy_setopt(http_server.curl, CURLOPT_HTTPHEADER, hs);

        curl_easy_setopt(http_server.curl, CURLOPT_WRITEFUNCTION, _discord_id_write_callback);
        curl_easy_setopt(http_server.curl, CURLOPT_WRITEDATA, (void *)&data);

        CURLcode result = curl_easy_perform(http_server.curl);
        free(header);

        if (result != CURLE_OK) {
            curl_easy_reset(http_server.curl);
            return result_error("Curl Error: %d", result);
        } else {
            struct json_object *info = json_tokener_parse(data.memory);

            struct json_object *err = json_object_object_get(info, "error");
            if (err) {
                const char *str = json_object_get_string(err);
                curl_easy_reset(http_server.curl);
                return result_error("Discord API: %s", str);
            }

            // Find User ID
            struct json_object *id = json_object_object_get(info, "id");
            if (!id || !(*out = json_object_get_uint64(id))) {
                curl_easy_reset(http_server.curl);
                return result_error("Discord API: Unable to find user id in response: %s", json_object_to_json_string_ext(info, JSON_C_TO_STRING_PRETTY ));
            }

            // Find Username
            struct json_object *username = json_object_object_get(info, "username");
            if (!id || !(*out2 = json_object_get_string(username))) {
                curl_easy_reset(http_server.curl);
                return result_error("Discord API: Unable to find username in response: %s", json_object_to_json_string_ext(info, JSON_C_TO_STRING_PRETTY ));
            }
        }
    }

    curl_easy_reset(http_server.curl);

    return result_ok();
}