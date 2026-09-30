# Aegis SDK for C++

Official client for the Aegis Authentication API. C++17 and libcurl only; the
JSON codec is bundled, so there are no other dependencies.

## Contents

```
include/aegis/aegis.hpp   Client, Options and AegisError
include/aegis/json.hpp    Bundled JSON value/parser
src/aegis.cpp             Implementation
examples/quickstart.cpp   Runnable sample application
CMakeLists.txt            Library + example build
```

## Build

No package registry — unzip and build the folder:

```bash
cmake -B build
cmake --build build
```

Consume it from another CMake project with `add_subdirectory(aegis-sdk-cpp)` and
`target_link_libraries(your_app PRIVATE aegis_sdk)`.

## Quickstart

```cpp
aegis::Options options;
options.baseUrl = "https://your-aegis-host";
options.appKey = std::getenv("AEGIS_APP_KEY");
options.version = "1.0.0";

aegis::Client client(options);
client.init();

auto auth = client.login("ada", password);
auto license = client.validateLicense("AEGS-4K7P-2M9X-QT31");
client.setVariable("last_level", "12");

client.startHeartbeat(std::chrono::seconds(60), [](const std::string& reason) { app.lock(reason); });
client.logout();
```

## Supported operations

`init`, `status`, `appData`, `registerUser`, `login`, `logout`, `heartbeat`,
`checkSession`, `isAuthenticated`, `useSession`, `userData`, `validateLicense`,
`activateLicense`, `getVariables`, `setVariable`, `checkVersion`, `downloads`,
`triggerWebhook`, plus `request()` for any endpoint added later.

## Error handling

```cpp
try {
    client.login(username, password);
} catch (const aegis::AegisError& error) {
    if (error.code() == "hwid_mismatch") ui.show("Locked to another machine.");
    else if (error.isNetworkError()) ui.show("Aegis is unreachable — retrying.");
    else throw;
}
```

## License

MIT — see `LICENSE`.
