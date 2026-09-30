# Aegis SDK for Swift

Official client for the Aegis Authentication API. Swift 5.9+, pure Foundation,
no third-party dependencies. Works on macOS 12+, iOS 15+, tvOS 15+, watchOS 8+
and Linux (swift-corelibs-foundation).

## Contents

```
Sources/Aegis/AegisClient.swift   Actor-isolated async client
Sources/Aegis/AegisOptions.swift  Configuration
Sources/Aegis/AegisError.swift    Typed errors
Sources/Aegis/HardwareId.swift    Stable machine identifier
Examples/Quickstart/              Sample application
Package.swift                     SwiftPM manifest
```

## Install

No package registry — unzip and add it as a local package:

```swift
.package(path: "Vendor/aegis-swift")
```

Or drag the folder into Xcode via _File → Add Package Dependencies → Add Local_.

## Quickstart

```swift
let aegis = AegisClient(options: AegisOptions(baseUrl: "https://your-aegis-host", appKey: appKey))
try await aegis.initialize()

let auth = try await aegis.login(username: "ada", password: password)
try await aegis.validateLicense("AEGS-4K7P-2M9X-QT31")
await aegis.startHeartbeat { reason in print("session ended: \(reason)") }
try await aegis.logout()
```

## Supported operations

`initialize`, `status`, `appData`, `register`, `login`, `logout`, `heartbeat`,
`startHeartbeat`/`stopHeartbeat`, `checkSession`, `isAuthenticated`,
`useSession`, `userData`, `validateLicense`, `activateLicense`, `variables`,
`setVariable`, `checkVersion`, `downloads`, `triggerWebhook`, plus `request()`
for any endpoint added later.

## Error handling

```swift
do {
    try await aegis.login(username: username, password: password)
} catch let error as AegisError where error.isLicenseError {
    show("License is not valid for this machine.")
} catch let error as AegisError where error.isNetworkError {
    show("Aegis is unreachable — retrying.")
}
```

## License

MIT — see `LICENSE`.
