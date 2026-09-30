# Aegis SDK for Dart & Flutter

Official client for the Aegis Authentication API. Dart 3, works in Flutter on
iOS, Android, desktop and web, and in plain Dart CLI apps.

## Contents

```
lib/aegis.dart              Public exports
lib/src/client.dart         Client with every API operation
lib/src/options.dart        Configuration
lib/src/exception.dart      Typed errors
lib/src/hardware_id.dart    Stable machine identifier
example/quickstart.dart     Sample application
pubspec.yaml                Package manifest
```

## Install

No package registry — unzip next to your app and use a path dependency:

```yaml
dependencies:
  aegis:
    path: vendor/aegis-dart
```

```bash
dart pub get
```

## Quickstart

```dart
final aegis = Aegis(AegisOptions(baseUrl: 'https://your-aegis-host', appKey: appKey));
await aegis.init();

final auth = await aegis.login(username: 'ada', password: password);
await aegis.validateLicense('AEGS-4K7P-2M9X-QT31');
aegis.startHeartbeat(onRevoked: (error) => signOut(error.message));
await aegis.logout();
```

## Supported operations

`init`, `status`, `appData`, `register`, `login`, `logout`, `heartbeat`,
`startHeartbeat`/`stopHeartbeat`, `checkSession`, `isAuthenticated`,
`useSession`, `userData`, `validateLicense`, `activateLicense`,
`getVariables`, `setVariable`, `checkVersion`, `downloads`, `triggerWebhook`,
plus `request()` for any endpoint added later.

## Error handling

```dart
try {
  await aegis.login(username: username, password: password);
} on AegisException catch (error) {
  if (error.isLicenseError) show('License is not valid for this machine.');
  else if (error.isNetworkError) show('Aegis is unreachable — retrying.');
  else rethrow;
}
```

> On Flutter web `HardwareId` is unavailable (`dart:io`); pass an explicit
> `hwid` in `AegisOptions` for web builds.

## License

MIT — see `LICENSE`.
