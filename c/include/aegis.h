/*
 * Aegis Authentication API client for C99.
 * Requires libcurl. All responses are returned as JSON text plus helper
 * accessors, so the SDK stays dependency-free beyond libcurl.
 */
#ifndef AEGIS_H
#define AEGIS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AEGIS_SDK_VERSION "1.0.0"

/** Result codes returned by every call. */
typedef enum {
    AEGIS_OK = 0,
    AEGIS_ERR_NETWORK = 1,
    AEGIS_ERR_API = 2,
    AEGIS_ERR_USAGE = 3,
    AEGIS_ERR_MEMORY = 4
} aegis_status;

/** Response payload. Always release with aegis_response_free(). */
typedef struct {
    aegis_status status;   /**< AEGIS_OK when the call succeeded. */
    long http_status;      /**< HTTP status, 0 on transport failure. */
    char *data;            /**< JSON text of the API `data` object. */
    char error_code[64];   /**< API error code, empty on success. */
    char error_message[256];
} aegis_response;

typedef struct aegis_client aegis_client;

/** Client configuration; zero-initialise then set the fields you need. */
typedef struct {
    const char *base_url;   /**< Required, e.g. "https://your-aegis-host". */
    const char *app_key;    /**< Required application key. */
    const char *api_key;    /**< Optional server API key. */
    const char *version;    /**< Client version, defaults to "1.0.0". */
    const char *channel;    /**< Update channel, defaults to "stable". */
    const char *hwid;       /**< Optional override; defaults to aegis_hardware_id(). */
    long timeout_seconds;   /**< Defaults to 20. */
    int max_retries;        /**< Defaults to 2. */
} aegis_options;

/* Lifecycle ------------------------------------------------------------- */
aegis_client *aegis_client_new(const aegis_options *options);
void aegis_client_free(aegis_client *client);
void aegis_response_free(aegis_response *response);

/** Stable, non-reversible machine identifier. Caller frees the result. */
char *aegis_hardware_id(void);

const char *aegis_session_token(const aegis_client *client);
void aegis_use_session(aegis_client *client, const char *token);

/** Extracts a string field from JSON text. Caller frees the result. */
char *aegis_json_string(const char *json, const char *key);
/** Extracts a boolean field from JSON text; returns fallback when absent. */
int aegis_json_bool(const char *json, const char *key, int fallback);

/* Generic call ---------------------------------------------------------- */
aegis_response aegis_request(aegis_client *client, const char *endpoint, const char *json_body);

/* Application ----------------------------------------------------------- */
aegis_response aegis_init(aegis_client *client);
aegis_response aegis_status_check(aegis_client *client);
aegis_response aegis_app_data(aegis_client *client);
aegis_response aegis_check_version(aegis_client *client, const char *version);
aegis_response aegis_downloads(aegis_client *client);

/* Authentication -------------------------------------------------------- */
aegis_response aegis_register(aegis_client *client, const char *username, const char *password,
                              const char *email, const char *license_key);
aegis_response aegis_login(aegis_client *client, const char *username, const char *password);
aegis_response aegis_logout(aegis_client *client);
aegis_response aegis_heartbeat(aegis_client *client);
aegis_response aegis_check_session(aegis_client *client);
int aegis_is_authenticated(aegis_client *client);
aegis_response aegis_user_data(aegis_client *client);

/* Licensing ------------------------------------------------------------- */
aegis_response aegis_validate_license(aegis_client *client, const char *license_key);
aegis_response aegis_activate_license(aegis_client *client, const char *license_key, const char *username);

/* Variables ------------------------------------------------------------- */
aegis_response aegis_get_variables(aegis_client *client, const char *scope, const char *license_key);
aegis_response aegis_set_variable(aegis_client *client, const char *scope, const char *key,
                                  const char *value, const char *license_key);

aegis_response aegis_trigger_webhook(aegis_client *client, const char *event, const char *json_payload);

#ifdef __cplusplus
}
#endif

#endif /* AEGIS_H */