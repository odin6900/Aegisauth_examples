/* Implementation of the Aegis C client. Requires libcurl. */
#include "aegis.h"

#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

struct aegis_client {
    char *base_url;
    char *app_key;
    char *api_key;
    char *version;
    char *channel;
    char *hwid;
    char *session_token;
    long timeout_seconds;
    int max_retries;
};

typedef struct {
    char *bytes;
    size_t length;
} buffer;

static char *dup_or_null(const char *value) {
    if (!value) return NULL;
    size_t len = strlen(value);
    char *copy = (char *)malloc(len + 1);
    if (copy) memcpy(copy, value, len + 1);
    return copy;
}

static size_t write_cb(char *ptr, size_t size, size_t nmemb, void *userdata) {
    buffer *buf = (buffer *)userdata;
    size_t chunk = size * nmemb;
    char *grown = (char *)realloc(buf->bytes, buf->length + chunk + 1);
    if (!grown) return 0;
    buf->bytes = grown;
    memcpy(buf->bytes + buf->length, ptr, chunk);
    buf->length += chunk;
    buf->bytes[buf->length] = '\0';
    return chunk;
}

static aegis_response make_error(aegis_status status, const char *code, const char *message, long http_status) {
    aegis_response response;
    memset(&response, 0, sizeof(response));
    response.status = status;
    response.http_status = http_status;
    snprintf(response.error_code, sizeof(response.error_code), "%s", code ? code : "");
    snprintf(response.error_message, sizeof(response.error_message), "%s", message ? message : "");
    return response;
}

/* --- tiny JSON helpers (flat and one-level nested lookups) --------------- */

static const char *find_key(const char *json, const char *key) {
    if (!json || !key) return NULL;
    char needle[128];
    snprintf(needle, sizeof(needle), "\"%s\"", key);
    const char *at = strstr(json, needle);
    if (!at) return NULL;
    at += strlen(needle);
    while (*at == ' ' || *at == ':') at++;
    return at;
}

char *aegis_json_string(const char *json, const char *key) {
    const char *at = find_key(json, key);
    if (!at || *at != '"') return NULL;
    at++;
    const char *start = at;
    size_t len = 0;
    while (*at && *at != '"') {
        if (*at == '\\' && at[1]) at++;
        at++;
        len++;
    }
    char *out = (char *)malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, start, len);
    out[len] = '\0';
    return out;
}

int aegis_json_bool(const char *json, const char *key, int fallback) {
    const char *at = find_key(json, key);
    if (!at) return fallback;
    if (strncmp(at, "true", 4) == 0) return 1;
    if (strncmp(at, "false", 5) == 0) return 0;
    return fallback;
}

/* --- lifecycle ----------------------------------------------------------- */

aegis_client *aegis_client_new(const aegis_options *options) {
    if (!options || !options->base_url || !options->app_key) return NULL;

    aegis_client *client = (aegis_client *)calloc(1, sizeof(aegis_client));
    if (!client) return NULL;

    client->base_url = dup_or_null(options->base_url);
    size_t len = client->base_url ? strlen(client->base_url) : 0;
    while (len > 0 && client->base_url[len - 1] == '/') client->base_url[--len] = '\0';

    client->app_key = dup_or_null(options->app_key);
    client->api_key = dup_or_null(options->api_key);
    client->version = dup_or_null(options->version ? options->version : "1.0.0");
    client->channel = dup_or_null(options->channel ? options->channel : "stable");
    client->hwid = options->hwid ? dup_or_null(options->hwid) : aegis_hardware_id();
    client->timeout_seconds = options->timeout_seconds > 0 ? options->timeout_seconds : 20;
    client->max_retries = options->max_retries > 0 ? options->max_retries : 2;

    curl_global_init(CURL_GLOBAL_DEFAULT);
    return client;
}

void aegis_client_free(aegis_client *client) {
    if (!client) return;
    free(client->base_url);
    free(client->app_key);
    free(client->api_key);
    free(client->version);
    free(client->channel);
    free(client->hwid);
    free(client->session_token);
    free(client);
}

void aegis_response_free(aegis_response *response) {
    if (!response) return;
    free(response->data);
    response->data = NULL;
}

char *aegis_hardware_id(void) {
    char facts[512] = {0};
#ifdef _WIN32
    char name[256] = {0};
    DWORD size = sizeof(name);
    GetComputerNameA(name, &size);
    const char *user = getenv("USERNAME");
    snprintf(facts, sizeof(facts), "%s|%s|windows", name, user ? user : "");
#else
    char host[256] = {0};
    gethostname(host, sizeof(host) - 1);
    const char *user = getenv("USER");
    snprintf(facts, sizeof(facts), "%s|%s|posix", host, user ? user : "");
#endif

    unsigned long long h = 1469598103934665603ULL;
    for (const unsigned char *p = (const unsigned char *)facts; *p; ++p) {
        h ^= *p;
        h *= 1099511628211ULL;
    }

    char *out = (char *)malloc(65);
    if (!out) return NULL;
    out[0] = '\0';
    for (int round = 0; round < 4; ++round) {
        h ^= h >> 33;
        h *= 0xff51afd7ed558ccdULL;
        char part[17];
        snprintf(part, sizeof(part), "%016llx", h);
        strncat(out, part, 64 - strlen(out));
    }
    return out;
}

const char *aegis_session_token(const aegis_client *client) {
    return client ? client->session_token : NULL;
}

void aegis_use_session(aegis_client *client, const char *token) {
    if (!client) return;
    free(client->session_token);
    client->session_token = dup_or_null(token);
}

static void store_session(aegis_client *client, const char *json) {
    char *token = aegis_json_string(json, "token");
    if (token) {
        free(client->session_token);
        client->session_token = token;
    }
}

/* --- transport ----------------------------------------------------------- */

aegis_response aegis_request(aegis_client *client, const char *endpoint, const char *json_body) {
    if (!client || !endpoint) return make_error(AEGIS_ERR_USAGE, "invalid_options", "Client and endpoint are required.", 0);

    char url[1024];
    snprintf(url, sizeof(url), "%s/api/public/v1/%s", client->base_url, endpoint);
    const char *payload = json_body ? json_body : "{}";

    for (int attempt = 0; attempt <= client->max_retries; ++attempt) {
        CURL *curl = curl_easy_init();
        if (!curl) return make_error(AEGIS_ERR_MEMORY, "network_error", "Unable to create an HTTP client.", 0);

        struct curl_slist *headers = NULL;
        char header[512];
        headers = curl_slist_append(headers, "content-type: application/json");
        headers = curl_slist_append(headers, "user-agent: aegis-c-sdk/" AEGIS_SDK_VERSION);
        snprintf(header, sizeof(header), "x-app-key: %s", client->app_key);
        headers = curl_slist_append(headers, header);
        if (client->api_key) {
            snprintf(header, sizeof(header), "x-api-key: %s", client->api_key);
            headers = curl_slist_append(headers, header);
        }
        if (client->session_token) {
            snprintf(header, sizeof(header), "x-session-token: %s", client->session_token);
            headers = curl_slist_append(headers, header);
        }

        buffer body = {NULL, 0};
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, client->timeout_seconds);

        CURLcode code = curl_easy_perform(curl);
        long http_status = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_status);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);

        if (code != CURLE_OK) {
            free(body.bytes);
            if (attempt < client->max_retries) continue;
            return make_error(AEGIS_ERR_NETWORK, "network_error", curl_easy_strerror(code), 0);
        }
        if (http_status >= 500 && attempt < client->max_retries) {
            free(body.bytes);
            continue;
        }

        const char *json = body.bytes ? body.bytes : "{}";
        if (!aegis_json_bool(json, "success", 0)) {
            char *error_code = aegis_json_string(json, "code");
            char *error_message = aegis_json_string(json, "message");
            aegis_response failure = make_error(AEGIS_ERR_API,
                                                error_code ? error_code : "server_error",
                                                error_message ? error_message : "Request failed.",
                                                http_status);
            free(error_code);
            free(error_message);
            free(body.bytes);
            return failure;
        }

        aegis_response response;
        memset(&response, 0, sizeof(response));
        response.status = AEGIS_OK;
        response.http_status = http_status;
        response.data = body.bytes;
        return response;
    }

    return make_error(AEGIS_ERR_NETWORK, "network_error", "Network request failed.", 0);
}

/* --- application ---------------------------------------------------------- */

aegis_response aegis_init(aegis_client *client) {
    char body[256];
    snprintf(body, sizeof(body), "{\"version\":\"%s\"}", client->version);
    return aegis_request(client, "init", body);
}

aegis_response aegis_status_check(aegis_client *client) { return aegis_request(client, "status", "{}"); }

aegis_response aegis_app_data(aegis_client *client) { return aegis_request(client, "app/data", "{}"); }

aegis_response aegis_downloads(aegis_client *client) { return aegis_request(client, "downloads", "{}"); }

aegis_response aegis_check_version(aegis_client *client, const char *version) {
    char body[320];
    snprintf(body, sizeof(body), "{\"version\":\"%s\",\"channel\":\"%s\"}",
             version ? version : client->version, client->channel);
    return aegis_request(client, "version/check", body);
}

/* --- authentication -------------------------------------------------------- */

aegis_response aegis_register(aegis_client *client, const char *username, const char *password,
                              const char *email, const char *license_key) {
    char body[1024];
    snprintf(body, sizeof(body),
             "{\"username\":\"%s\",\"password\":\"%s\",\"hwid\":\"%s\"%s%s%s%s%s%s}",
             username, password, client->hwid,
             email ? ",\"email\":\"" : "", email ? email : "", email ? "\"" : "",
             license_key ? ",\"license_key\":\"" : "", license_key ? license_key : "", license_key ? "\"" : "");
    aegis_response response = aegis_request(client, "register", body);
    if (response.status == AEGIS_OK) store_session(client, response.data);
    return response;
}

aegis_response aegis_login(aegis_client *client, const char *username, const char *password) {
    char body[768];
    snprintf(body, sizeof(body), "{\"username\":\"%s\",\"password\":\"%s\",\"hwid\":\"%s\"}",
             username, password, client->hwid);
    aegis_response response = aegis_request(client, "login", body);
    if (response.status == AEGIS_OK) store_session(client, response.data);
    return response;
}

aegis_response aegis_logout(aegis_client *client) {
    aegis_response response = aegis_request(client, "logout", "{}");
    aegis_use_session(client, NULL);
    return response;
}

aegis_response aegis_heartbeat(aegis_client *client) { return aegis_request(client, "heartbeat", "{}"); }

aegis_response aegis_check_session(aegis_client *client) { return aegis_request(client, "session/check", "{}"); }

int aegis_is_authenticated(aegis_client *client) {
    if (!client || !client->session_token) return 0;
    aegis_response response = aegis_check_session(client);
    int valid = response.status == AEGIS_OK && aegis_json_bool(response.data, "valid", 0);
    aegis_response_free(&response);
    return valid;
}

aegis_response aegis_user_data(aegis_client *client) { return aegis_request(client, "user/data", "{}"); }

/* --- licensing -------------------------------------------------------------- */

aegis_response aegis_validate_license(aegis_client *client, const char *license_key) {
    char body[512];
    snprintf(body, sizeof(body), "{\"license_key\":\"%s\",\"hwid\":\"%s\"}", license_key, client->hwid);
    return aegis_request(client, "license/validate", body);
}

aegis_response aegis_activate_license(aegis_client *client, const char *license_key, const char *username) {
    char body[768];
    snprintf(body, sizeof(body), "{\"license_key\":\"%s\",\"hwid\":\"%s\"%s%s%s}",
             license_key, client->hwid,
             username ? ",\"username\":\"" : "", username ? username : "", username ? "\"" : "");
    return aegis_request(client, "license/activate", body);
}

/* --- variables --------------------------------------------------------------- */

aegis_response aegis_get_variables(aegis_client *client, const char *scope, const char *license_key) {
    char body[512];
    snprintf(body, sizeof(body), "{\"scope\":\"%s\"%s%s%s}",
             scope ? scope : "application",
             license_key ? ",\"license_key\":\"" : "", license_key ? license_key : "", license_key ? "\"" : "");
    return aegis_request(client, "variables/get", body);
}

aegis_response aegis_set_variable(aegis_client *client, const char *scope, const char *key,
                                  const char *value, const char *license_key) {
    char body[1024];
    snprintf(body, sizeof(body), "{\"scope\":\"%s\",\"key\":\"%s\",\"value\":\"%s\"%s%s%s}",
             scope ? scope : "user", key, value,
             license_key ? ",\"license_key\":\"" : "", license_key ? license_key : "", license_key ? "\"" : "");
    return aegis_request(client, "variables/set", body);
}

aegis_response aegis_trigger_webhook(aegis_client *client, const char *event, const char *json_payload) {
    char body[2048];
    snprintf(body, sizeof(body), "{\"event\":\"%s\",\"payload\":%s}", event, json_payload ? json_payload : "{}");
    return aegis_request(client, "webhook/trigger", body);
}