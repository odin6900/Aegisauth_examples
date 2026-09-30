# AegisAuth — C# / .NET SDK

Official .NET client for the **Aegis Authentication** platform.

- **Single file** — the whole SDK is [`AegisAuth.cs`](AegisAuth.cs). Drop it into any project.
- **Zero dependencies** — no NuGet packages. Uses only `System.Net.Http` and `System.Runtime.Serialization`.
- **Any target** — .NET Framework 4.7.2+, .NET 6/7/8+, WinForms, WPF, console, ASP.NET, Unity (with .NET Standard 2.0 profile).

## Install

1. Download the SDK archive from your Aegis dashboard (**Developers → SDKs → C#**).
2. Copy `AegisAuth.cs` into your project.
3. Done — there is nothing else to reference.

## Quick start

```csharp
AegisAuth.api AegisApp = new AegisAuth.api(
    name:    "My Application",       // display name of your application
    ownerid: "YOUR-APPLICATION-KEY", // Application Key from the dashboard
    secret:  "",                     // optional API key (elevated calls only)
    version: "1.0.0"                 // version of the build you ship
);

AegisApp.init();
if (!AegisApp.response.success)
    Console.WriteLine(AegisApp.response.message);

AegisApp.login("ada", "correct horse battery staple");
if (AegisApp.response.success)
    Console.WriteLine($"Welcome {AegisApp.user_data.username}");
```

### Constructor parameters

| Parameter | Maps to                                 | Notes                                            |
| --------- | --------------------------------------- | ------------------------------------------------ |
| `name`    | Application name in the dashboard       | Informational                                    |
| `ownerid` | **Application Key** (public key)        | Identifies your application; sent as `x-app-key` |
| `secret`  | API key (dashboard → Developers → Keys) | Optional; only needed for elevated calls         |
| `version` | The build you are shipping              | Checked against the minimum published version    |
| `url`     | _(optional)_ API base URL               | Defaults to the hosted Aegis platform            |

## API

| Method                                 | Purpose                                                              |
| -------------------------------------- | -------------------------------------------------------------------- |
| `init()`                               | Handshake. Call once before any other operation. Fills `app_data`.   |
| `register(user, pass, key?, email?)`   | Create a user, optionally redeeming a license key.                   |
| `login(user, pass)`                    | Authenticate and open a session.                                     |
| `license(key)`                         | Key-only authentication — validates and binds a key to this machine. |
| `upgrade(user, key)`                   | Attach a license key to an existing account.                         |
| `var(varid)`                           | Read an application variable published from the dashboard.           |
| `getvar(name)` / `setvar(name, value)` | Per-user variables (session required).                               |
| `log(msg)`                             | Write to the application's audit log.                                |
| `check()`                              | Validate the active session against the server.                      |
| `logout()`                             | Terminate the session server-side.                                   |
| `fetchstats()`                         | Refresh `app_data` (user/key counters, downloads).                   |
| `update_available(channel?)`           | True when a newer build is published.                                |
| `use_session(token)`                   | Restore a persisted session token.                                   |
| `expirydaysleft()`                     | Days left on the active license.                                     |

Every call fills `AegisApp.response.success` and `AegisApp.response.message`.

## Session & user data

```csharp
AegisApp.user_data.username        // signed-in user
AegisApp.user_data.createdate      // DateTime? account creation
AegisApp.user_data.subscriptions   // active license(s) with expiry + timeleft
AegisApp.app_data.numUsers         // total users on the application
AegisApp.app_data.numKeys          // total license keys
AegisApp.app_data.app_ver          // latest published version
```

Persist `AegisApp.sessionid` (e.g. in `Properties.Settings`) and restore it with
`AegisApp.use_session(token)` followed by `AegisApp.check()` to skip the login screen.

## Example

A complete console application lives in [`examples/Quickstart`](examples/Quickstart):

```bash
dotnet run --project examples/Quickstart
```

## Error handling

The SDK never throws for API failures — check `response.success` after each call:

```csharp
AegisApp.login(user, pass);
if (!AegisApp.response.success)
{
    MessageBox.Show(AegisApp.response.message, "Login failed");
    return;
}
```

Network problems surface as `response.message = "Network error: ..."`.

## Versioning

This SDK follows [SemVer](https://semver.org). See [CHANGELOG.md](CHANGELOG.md).
