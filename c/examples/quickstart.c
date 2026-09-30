/*
 * Runnable quickstart for the Aegis C SDK.
 *
 *   make
 *   AEGIS_APP_KEY=... ./quickstart
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aegis.h"

static const char *env_or(const char *key, const char *fallback) {
    const char *value = getenv(key);
    return (value && *value) ? value : fallback;
}

static int check(aegis_response *response, const char *label) {
    if (response->status != AEGIS_OK) {
        fprintf(stderr, "%s failed [%s] %s\n", label, response->error_code, response->error_message);
        aegis_response_free(response);
        return 0;
    }
    return 1;
}

int main(void) {
    aegis_options options;
    memset(&options, 0, sizeof(options));
    options.base_url = env_or("AEGIS_BASE_URL", "http://localhost:8080");
    options.app_key = env_or("AEGIS_APP_KEY", "");
    options.version = "1.0.0";

    aegis_client *client = aegis_client_new(&options);
    if (!client) {
        fprintf(stderr, "Unable to create the Aegis client.\n");
        return 1;
    }

    aegis_response info = aegis_init(client);
    if (!check(&info, "init")) { aegis_client_free(client); return 1; }
    printf("initialized: %s\n", info.data);
    aegis_response_free(&info);

    aegis_response auth = aegis_login(client, env_or("AEGIS_USERNAME", "demo"), env_or("AEGIS_PASSWORD", "demo-password"));
    if (!check(&auth, "login")) { aegis_client_free(client); return 1; }
    char *username = aegis_json_string(auth.data, "username");
    printf("signed in as %s\n", username ? username : "(unknown)");
    free(username);
    aegis_response_free(&auth);

    aegis_response vars = aegis_get_variables(client, "user", NULL);
    if (check(&vars, "variables/get")) {
        printf("variables: %s\n", vars.data);
        aegis_response_free(&vars);
    }

    aegis_response dl = aegis_downloads(client);
    if (check(&dl, "downloads")) {
        printf("downloads: %s\n", dl.data);
        aegis_response_free(&dl);
    }

    printf("authenticated: %s\n", aegis_is_authenticated(client) ? "yes" : "no");

    aegis_response bye = aegis_logout(client);
    aegis_response_free(&bye);
    aegis_client_free(client);
    printf("signed out.\n");
    return 0;
}