# Aegis SDK for C

Official client for the Aegis Authentication API. C99 and libcurl only.

## Contents

```
include/aegis.h        Full public API
src/aegis.c            Implementation
examples/quickstart.c  Runnable sample application
Makefile               Static library + example build
```

## Build

No package registry — unzip and build the folder:

```bash
make            # produces libaegis.a and ./quickstart
```

Link with `-laegis -lcurl` and add `include/` to your include path.

## Quickstart

```c
aegis_options options = {0};
options.base_url = "https://your-aegis-host";
options.app_key = getenv("AEGIS_APP_KEY");
options.version = "1.0.0";

aegis_client *client = aegis_client_new(&options);
aegis_response info = aegis_init(client);
aegis_response_free(&info);

aegis_response auth = aegis_login(client, "ada", password);
if (auth.status != AEGIS_OK) fprintf(stderr, "[%s] %s\n", auth.error_code, auth.error_message);
aegis_response_free(&auth);

aegis_client_free(client);
```

## Supported operations

`aegis_init`, `aegis_status_check`, `aegis_app_data`, `aegis_register`,
`aegis_login`, `aegis_logout`, `aegis_heartbeat`, `aegis_check_session`,
`aegis_is_authenticated`, `aegis_use_session`, `aegis_user_data`,
`aegis_validate_license`, `aegis_activate_license`, `aegis_get_variables`,
`aegis_set_variable`, `aegis_check_version`, `aegis_downloads`,
`aegis_trigger_webhook`, plus `aegis_request()` for any endpoint added later.

## Error handling

Every call returns an `aegis_response`. Check `status`, then read `error_code`
and `error_message`; always call `aegis_response_free()` when done.

## License

MIT — see `LICENSE`.
