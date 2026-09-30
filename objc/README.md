# Aegis SDK for Objective-C

Official client for the Aegis Authentication API. Pure Foundation
(`NSURLSession` + `NSJSONSerialization`), ARC, no third-party dependencies.
Targets macOS 10.15+ and iOS 13+.

## Contents

```
Aegis/include/AegisClient.h    Public client interface
Aegis/include/AegisOptions.h   Configuration
Aegis/include/AegisError.h     NSError domain + classification helpers
Aegis/*.m                      Implementation
Examples/quickstart.m          Sample application
Makefile                       Static library + example build
```

## Install

No package registry — unzip and drag the `Aegis` folder into your Xcode target,
adding `Aegis/include` to _Header Search Paths_. Or build a static library:

```bash
make          # libaegis.a
make example  # ./quickstart
```

## Quickstart

```objc
AegisOptions *options = [AegisOptions optionsWithBaseUrl:@"https://your-aegis-host" appKey:appKey];
AegisClient *aegis = [[AegisClient alloc] initWithOptions:options];

[aegis initializeApplication:^(NSDictionary *info, NSError *error) {
    [aegis loginUsername:@"ada" password:password completion:^(NSDictionary *auth, NSError *loginError) {
        if (loginError.aegis_isAuthError) { return; }
        [aegis startHeartbeatWithInterval:60 onRevoked:^(NSError *revoked) { /* sign out */ }];
    }];
}];
```

## Supported operations

`initializeApplication`, `status`, `appData`, `registerUsername:…`,
`loginUsername:…`, `logout`, `heartbeat`, `startHeartbeatWithInterval:`,
`stopHeartbeat`, `checkSession`, `isAuthenticated`, `useSession`, `userData`,
`validateLicense`, `activateLicense`, `variablesForScope:`, `setVariable:`,
`checkVersion`, `downloads`, `triggerWebhook`, plus `request:body:completion:`
for any endpoint added later.

## Error handling

Every failure is an `NSError` in the `AegisErrorDomain` with helpers:

```objc
if (error.aegis_isLicenseError) { show(@"License is not valid for this machine."); }
else if (error.aegis_isNetworkError) { show(@"Aegis is unreachable — retrying."); }
NSLog(@"%@", error.aegis_code);
```

## License

MIT — see `LICENSE`.
