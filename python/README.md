# AegisAuth — Python SDK

Official Python client for the **Aegis Authentication** platform.

- **Single file** — the whole SDK is [`aegis_auth.py`](aegis_auth.py).
- **Zero dependencies** — standard library only (`urllib`, no `requests` needed).
- **Anywhere Python runs** — CPython 3.8+ on Windows, macOS and Linux.

## Install

1. Download the SDK archive from your Aegis dashboard (**Developers → SDKs → Python**).
2. Copy `aegis_auth.py` into your project.
3. `from aegis_auth import AegisAuth` — done.

## Quick start

```python
from aegis_auth import AegisAuth

aegis_app = AegisAuth(
    name="My Application",           # display name of your application
    ownerid="YOUR-APPLICATION-KEY",  # Application Key from the dashboard
    secret="",                       # optional API key (elevated calls only)
    version="1.0.0",                 # version of the build you ship
)

aegis_app.init()
if not aegis_app.response.success:
    raise SystemExit(aegis_app.response.message)

aegis_app.login("ada", "correct horse battery staple")
if aegis_app.response.success:
    print(f"Welcome {aegis_app.user_data.username}")
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

| Method                                                   | Purpose                                                              |
| -------------------------------------------------------- | -------------------------------------------------------------------- |
| `init()`                                                 | Handshake. Call once before any other operation. Fills `app_data`.   |
| `register(user, password, license_key=None, email=None)` | Create a user, optionally redeeming a key.                           |
| `login(user, password)`                                  | Authenticate and open a session.                                     |
| `license(key)`                                           | Key-only authentication — validates and binds a key to this machine. |
| `upgrade(user, key)`                                     | Attach a license key to an existing account.                         |
| `var(varid)`                                             | Read an application variable published from the dashboard.           |
| `getvar(name)` / `setvar(name, value)`                   | Per-user variables (session required).                               |
| `log(message)`                                           | Write to the application's audit log.                                |
| `check()`                                                | Validate the active session against the server.                      |
| `logout()`                                               | Terminate the session server-side.                                   |
| `fetchstats()`                                           | Refresh `app_data` (user/key counters, downloads).                   |
| `update_available(channel="stable")`                     | True when a newer build is published.                                |
| `use_session(token)`                                     | Restore a persisted session token.                                   |
| `expirydaysleft()`                                       | Days left on the active license.                                     |

Every call fills `aegis_app.response.success` and `aegis_app.response.message`.

## Session & user data

```python
aegis_app.user_data.username        # signed-in user
aegis_app.user_data.subscriptions   # active license(s) with expiry + timeleft
aegis_app.app_data.numUsers         # total users on the application
aegis_app.app_data.numKeys          # total license keys
aegis_app.app_data.app_ver          # latest published version
```

Persist `aegis_app.sessionid` and restore it with `use_session(token)` + `check()`
to skip the login screen.

## Example

```bash
python examples/quickstart.py
```

## Error handling

The SDK never raises for API failures — check `response.success`:

```python
aegis_app.login(user, password)
if not aegis_app.response.success:
    print("Login failed:", aegis_app.response.message)
```

Network problems surface as `response.message = "Network error: ..."`.

## Versioning

This SDK follows [SemVer](https://semver.org). See [CHANGELOG.md](CHANGELOG.md).
