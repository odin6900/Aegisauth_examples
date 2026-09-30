# Aegis SDK for Java

Official client for the Aegis Authentication API. JDK 17+, zero third-party
dependencies (uses `java.net.http` and a bundled minimal JSON codec).

## Contents

```
src/main/java/io/aegis/sdk/Aegis.java           Client with every API operation
src/main/java/io/aegis/sdk/AegisOptions.java    Configuration builder
src/main/java/io/aegis/sdk/AegisException.java  Typed error codes
src/main/java/io/aegis/sdk/HardwareId.java      Stable hardware id
src/main/java/io/aegis/sdk/Json.java            Dependency-free JSON codec
examples/                                       Runnable quickstart
```

## Install

No registry needed — unzip and build the folder:

```bash
mvn -q package        # target/aegis-sdk-1.0.0.jar
```

Then add the jar to your classpath, or install it locally with
`mvn install:install-file`.

## Quickstart

```java
try (Aegis aegis = new Aegis(new AegisOptions("https://your-aegis-host", appKey).version("1.0.0"))) {
    Map<String, Object> info = aegis.init();

    Map<String, Object> auth = aegis.login("ada", password);
    System.out.println("signed in as " + ((Map<?, ?>) auth.get("user")).get("username"));

    Map<String, Object> check = aegis.validateLicense("AEGS-4K7P-2M9X-QT31");
    System.out.println(check.get("valid"));

    aegis.setVariable("last_level", "12", "user", null);
    System.out.println(aegis.getVariables("user", null).get("variables"));

    aegis.startHeartbeat(Duration.ofSeconds(60), reason -> app.lock(reason));
    aegis.logout();
}
```

## Supported operations

`init`, `status`, `appData`, `register`, `login`, `logout`, `heartbeat`,
`checkSession`, `isAuthenticated`, `useSession`, `userData`,
`validateLicense`, `activateLicense`, `getVariables`, `setVariable`,
`checkVersion`, `triggerWebhook`, plus `request()` for any endpoint added later.

## Error handling

```java
try {
    aegis.login(username, password);
} catch (AegisException error) {
    if ("hwid_mismatch".equals(error.getCode())) ui.show("Locked to another machine.");
    else if (error.isNetworkError()) ui.show("Aegis is unreachable — retrying.");
    else throw error;
}
```

## License

MIT — see `LICENSE`.
