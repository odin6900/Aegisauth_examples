# Aegis SDK for Kotlin

Official client for the Aegis Authentication API. JVM 17+, zero third-party
dependencies (JDK `HttpClient` plus a bundled JSON codec).

## Contents

```
src/main/kotlin/io/aegis/sdk/Aegis.kt            Client
src/main/kotlin/io/aegis/sdk/AegisException.kt   Typed errors
src/main/kotlin/io/aegis/sdk/Json.kt             Bundled JSON codec
src/main/kotlin/io/aegis/sdk/examples/           Sample application
build.gradle.kts                                 Gradle build
```

## Install

No package registry — unzip and either open the folder in your IDE or add the
sources to an existing Gradle module:

```kotlin
// settings.gradle.kts
includeBuild("libs/aegis-kotlin")
```

```bash
./gradlew build
```

## Quickstart

```kotlin
val aegis = Aegis(AegisOptions(baseUrl = "https://your-aegis-host", appKey = APP_KEY, version = "1.0.0"))
aegis.init()

val auth = aegis.login("ada", password)
println(auth.obj("user").str("username"))

aegis.validateLicense("AEGS-4K7P-2M9X-QT31")
aegis.startHeartbeat { reason -> println("session ended: $reason") }
aegis.logout()
```

## Supported operations

`init`, `status`, `appData`, `register`, `login`, `logout`, `heartbeat`,
`startHeartbeat`/`stopHeartbeat`, `checkSession`, `isAuthenticated`,
`useSession`, `userData`, `validateLicense`, `activateLicense`,
`getVariables`, `setVariable`, `checkVersion`, `downloads`, `triggerWebhook`,
plus `request()` for any endpoint added later.

## Error handling

```kotlin
try {
    aegis.login(username, password)
} catch (error: AegisException) {
    when {
        error.isLicenseError -> ui.show("License is not valid for this machine.")
        error.isNetworkError -> ui.show("Aegis is unreachable — retrying.")
        else -> throw error
    }
}
```

## License

MIT — see `LICENSE`.
