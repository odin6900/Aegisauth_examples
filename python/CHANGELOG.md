# Changelog — AegisAuth Python SDK

All notable changes to this SDK are documented here. The project follows
[Semantic Versioning](https://semver.org).

## [1.0.0] - 2026-01-15

### Added

- Single-file, zero-dependency `aegis_auth.py` (class `AegisAuth`, stdlib `urllib` transport).
- Full authentication flow: `init`, `register`, `login`, `license` (key-only), `upgrade`, `logout`.
- Session helpers: `check`, `use_session`, `expirydaysleft`, per-user `getvar`/`setvar`.
- Application services: `var`, `log`, `fetchstats`, `update_available`.
- `user_data` / `app_data` / `response` containers with subscription expiry helpers.
- Hashed hardware id for Windows (WMIC UUID), macOS (IOPlatformUUID) and Linux (`/etc/machine-id`).
- `examples/quickstart.py` interactive console application.
